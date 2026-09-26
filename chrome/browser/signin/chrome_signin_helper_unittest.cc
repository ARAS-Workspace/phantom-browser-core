// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_helper.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "build/buildflag.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/signin/android/signin_bridge_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

#if BUILDFLAG(ENABLE_MIRROR)
const char kMirrorActionAddSession[] = "action=ADDSESSION";
#endif

// URLRequestInterceptor adding a account consistency response header to Gaia
// responses.
class TestRequestInterceptor : public net::URLRequestInterceptor {
 public:
  explicit TestRequestInterceptor(const std::string& header_name,
                                  const std::string& header_value)
      : header_name_(header_name), header_value_(header_value) {}
  ~TestRequestInterceptor() override = default;

 private:
  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    std::string response_headers =
        base::StringPrintf("HTTP/1.1 200 OK\n\n%s: %s\n", header_name_.c_str(),
                           header_value_.c_str());
    return std::make_unique<net::URLRequestTestJob>(request, response_headers,
                                                    "", true);
  }

  const std::string header_name_;
  const std::string header_value_;
};

class TestResponseAdapter : public signin::ResponseAdapter,
                            public base::SupportsUserData {
 public:
  TestResponseAdapter(const std::string& header_name,
                      const std::string& header_value,
                      bool is_outermost_main_frame,
                      content::WebContents* web_contents = nullptr)
      : is_outermost_main_frame_(is_outermost_main_frame),
        headers_(new net::HttpResponseHeaders(std::string())),
        web_contents_(web_contents) {
    headers_->SetHeader(header_name, header_value);
  }

  TestResponseAdapter(const TestResponseAdapter&) = delete;
  TestResponseAdapter& operator=(const TestResponseAdapter&) = delete;

  ~TestResponseAdapter() override = default;

  content::WebContents::Getter GetWebContentsGetter() const override {
    return base::BindLambdaForTesting(
        [contents = web_contents_]() -> content::WebContents* {
          return contents;
        });
  }
  bool IsOutermostMainFrame() const override {
    return is_outermost_main_frame_;
  }
  GURL GetUrl() const override { return GURL("https://accounts.google.com"); }
  std::optional<url::Origin> GetRequestInitiator() const override {
    // Pretend the request came from the same origin.
    return url::Origin::Create(GetUrl());
  }
  const url::Origin* GetRequestTopFrameOrigin() const override {
    return &request_top_frame_origin_;
  }
  const net::HttpResponseHeaders* GetHeaders() const override {
    return headers_.get();
  }

  void RemoveHeader(const std::string& name) override {
    headers_->RemoveHeader(name);
  }

  base::SupportsUserData::Data* GetUserData(const void* key) const override {
    return base::SupportsUserData::GetUserData(key);
  }

  void SetUserData(
      const void* key,
      std::unique_ptr<base::SupportsUserData::Data> data) override {
    return base::SupportsUserData::SetUserData(key, std::move(data));
  }

 private:
  bool is_outermost_main_frame_;
  const url::Origin request_top_frame_origin_{url::Origin::Create(GetUrl())};
  scoped_refptr<net::HttpResponseHeaders> headers_;
  raw_ptr<content::WebContents> web_contents_;
};

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD3(OpenURLFromTab,
               content::WebContents*(
                   content::WebContents*,
                   const content::OpenURLParams&,
                   base::OnceCallback<void(content::NavigationHandle&)>));
};

class TestChromeRequestAdapter : public signin::ChromeRequestAdapter {
 public:
  explicit TestChromeRequestAdapter(const GURL& url)
      : signin::ChromeRequestAdapter(url,
                                     original_headers_,
                                     &modified_headers_,
                                     &headers_to_remove_) {}

  const net::HttpRequestHeaders& modified_headers() const {
    return modified_headers_;
  }

  // ChromeRequestAdapter:
  content::WebContents::Getter GetWebContentsGetter() const override {
    return content::WebContents::Getter();
  }
  network::mojom::RequestDestination GetRequestDestination() const override {
    return network::mojom::RequestDestination::kDocument;
  }
  bool IsOutermostMainFrame() const override { return true; }
  bool IsFetchLikeAPI() const override { return false; }
  GURL GetReferrer() const override { return GURL(); }
  void SetDestructionCallback(base::OnceClosure closure) override {}

 private:
  net::HttpRequestHeaders original_headers_;
  net::HttpRequestHeaders modified_headers_;
  std::vector<std::string> headers_to_remove_;
};

}  // namespace

using ::testing::_;
using ::testing::Eq;

class ChromeSigninHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  ChromeSigninHelperTest() = default;
  ~ChromeSigninHelperTest() override = default;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

// Tests that Dice response headers are removed after being processed.
TEST_F(ChromeSigninHelperTest, RemoveDiceSigninHeader) {
  // Process the header.
  TestResponseAdapter adapter(signin::kDiceResponseHeader, "Foo",
                              /*is_outermost_main_frame=*/false);
  signin::ProcessAccountConsistencyResponseHeaders(&adapter, GURL(),
                                                   /*is_off_the_record=*/false);

  // Check that the header has been removed.
  EXPECT_FALSE(adapter.GetHeaders()->HasHeader(signin::kDiceResponseHeader));
}

TEST_F(ChromeSigninHelperTest, FixAccountConsistencyRequestHeader) {
  // Setup the test environment.
  sync_preferences::TestingPrefServiceSyncable prefs;
  content_settings::CookieSettings::RegisterProfilePrefs(prefs.registry());
  HostContentSettingsMap::RegisterProfilePrefs(prefs.registry());
  privacy_sandbox::RegisterProfilePrefs(prefs.registry());
  scoped_refptr<HostContentSettingsMap> settings_map =
      new HostContentSettingsMap(&prefs, /*is_off_the_record=*/false,
                                 /*store_last_modified=*/false,
                                 /*restore_session=*/false,
                                 /*should_record_metrics=*/false);
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      new content_settings::CookieSettings(
          settings_map.get(), &prefs, /*is_incognito=*/false,
          content_settings::CookieSettings::
              NoFedCmSharingPermissionsCallback());

  {
    // Non-elligible request, no header.
    TestChromeRequestAdapter request(GURL("https://gmail.com"));
    signin::FixAccountConsistencyRequestHeader(
        &request, GURL(), /*is_off_the_record=*/false,
        /*incognito_availability=*/0, signin::AccountConsistencyMethod::kDice,
        GaiaId("gaia_id"), signin::ConsentLevel::kSignin,
        /*is_child_account=*/signin::Tribool::kFalse,
        /*is_sync_feature_enabled=*/true, "device_id", cookie_settings.get());
    EXPECT_EQ(
        request.modified_headers().GetHeader(signin::kChromeConnectedHeader),
        std::nullopt);
  }

  {
    // Google Docs gets the header.
    TestChromeRequestAdapter request(GURL("https://docs.google.com"));
    signin::FixAccountConsistencyRequestHeader(
        &request, GURL(), /*is_off_the_record=*/false,
        /*incognito_availability=*/0, signin::AccountConsistencyMethod::kDice,
        GaiaId("gaia_id"), signin::ConsentLevel::kSignin,
        /*is_child_account=*/signin::Tribool::kFalse,
        /*is_sync_feature_enabled=*/true, "device_id", cookie_settings.get());
    std::string expected_header =
        "source=Chrome,id=gaia_id,mode=0,enable_account_consistency=false,"
        "supervised=false,consistency_enabled_by_default=false";
    EXPECT_THAT(
        request.modified_headers().GetHeader(signin::kChromeConnectedHeader),
        testing::Optional(expected_header));
  }

  // Tear down the test environment.
  settings_map->ShutdownOnUIThread();
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_MIRROR)
// Tests that user data is set on Mirror requests.
TEST_F(ChromeSigninHelperTest, MirrorMainFrame) {
  // Process the header.
  TestResponseAdapter response_adapter(signin::kChromeManageAccountsHeader,
                                       kMirrorActionAddSession,
                                       /*is_outermost_main_frame=*/true);
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   /*is_off_the_record=*/false);
  // Check that the header has not been removed.
  EXPECT_TRUE(response_adapter.GetHeaders()->HasHeader(
      signin::kChromeManageAccountsHeader));
  // Request was flagged with the user data.
  EXPECT_TRUE(response_adapter.GetUserData(
      signin::kManageAccountsHeaderReceivedUserDataKey));
}

// Tests that user data is not set on Mirror requests for sub frames.
TEST_F(ChromeSigninHelperTest, MirrorSubFrame) {
  // Process the header.
  TestResponseAdapter response_adapter(signin::kChromeManageAccountsHeader,
                                       kMirrorActionAddSession,
                                       /*is_outermost_main_frame=*/false);
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   /*is_off_the_record=*/false);
  // Request was not flagged with the user data.
  EXPECT_FALSE(response_adapter.GetUserData(
      signin::kManageAccountsHeaderReceivedUserDataKey));
}

TEST_F(ChromeSigninHelperTest, NonEligibleURL) {
  // Non-eligible request, no header.
  TestChromeRequestAdapter request(GURL("https://gmail.com"));
  signin::FixAccountConsistencyRequestHeader(
      &request, GURL(), /*is_off_the_record=*/false,
      /*incognito_availability=*/0, signin::AccountConsistencyMethod::kMirror,
      GaiaId("gaia_id"), signin::ConsentLevel::kSignin,
      /*is_child_account=*/signin::Tribool::kFalse,
      /*is_sync_feature_enabled=*/false,
      CookieSettingsFactory::GetForProfile(profile()).get());
  EXPECT_EQ(
      request.modified_headers().GetHeader(signin::kChromeConnectedHeader),
      std::nullopt);
}

TEST_F(ChromeSigninHelperTest, EligibleURL) {
  // Google Docs is eligible for the Mirror header.
  TestChromeRequestAdapter request(GURL("https://docs.google.com"));
  signin::FixAccountConsistencyRequestHeader(
      &request, GURL(), /*is_off_the_record=*/false,
      /*incognito_availability=*/0, signin::AccountConsistencyMethod::kMirror,
      GaiaId("gaia_id"), signin::ConsentLevel::kSignin,
      /*is_child_account=*/signin::Tribool::kFalse,
      /*is_sync_feature_enabled=*/false,
      CookieSettingsFactory::GetForProfile(profile()).get());
  std::string expected_header =
      "source=Chrome,id=gaia_id,mode=0,enable_account_consistency=true,"
      "supervised=false,consistency_enabled_by_default=false";
  EXPECT_THAT(
      request.modified_headers().GetHeader(signin::kChromeConnectedHeader),
      testing::Optional(expected_header));
}

TEST_F(ChromeSigninHelperTest, MirrorConsentLevelSync) {
  // Google Docs is eligible for the Mirror header.
  TestChromeRequestAdapter request(GURL("https://docs.google.com"));

  // 1. ConsentLevel::kSync, is_sync_feature_enabled=false.
  signin::FixAccountConsistencyRequestHeader(
      &request, GURL(), /*is_off_the_record=*/false,
      /*incognito_availability=*/0, signin::AccountConsistencyMethod::kMirror,
      GaiaId("gaia_id"), signin::ConsentLevel::kSync,
      /*is_child_account=*/signin::Tribool::kFalse,
      /*is_sync_feature_enabled=*/false,
      CookieSettingsFactory::GetForProfile(profile()).get());

  // On other platforms, no header is added since sync is disabled and consent
  // level is kSync.
  EXPECT_EQ(
      request.modified_headers().GetHeader(signin::kChromeConnectedHeader),
      std::nullopt);

  // 2. ConsentLevel::kSync, is_sync_feature_enabled=true.
  TestChromeRequestAdapter request_sync(GURL("https://docs.google.com"));
  signin::FixAccountConsistencyRequestHeader(
      &request_sync, GURL(), /*is_off_the_record=*/false,
      /*incognito_availability=*/0, signin::AccountConsistencyMethod::kMirror,
      GaiaId("gaia_id"), signin::ConsentLevel::kSync,
      /*is_child_account=*/signin::Tribool::kFalse,
      /*is_sync_feature_enabled=*/true,
      CookieSettingsFactory::GetForProfile(profile()).get());

  std::string expected_header_sync =
      "source=Chrome,id=gaia_id,mode=0,enable_account_consistency=true,"
      "supervised=false,consistency_enabled_by_default=false";
  EXPECT_THAT(
      request_sync.modified_headers().GetHeader(signin::kChromeConnectedHeader),
      testing::Optional(expected_header_sync));
}

TEST_F(ChromeSigninHelperTest, NonDefaultGaiaOrigin) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGaiaUrl, "http://example.com");
  auto gaia_urls_override = std::make_unique<GaiaUrls>();
  GaiaUrls::SetInstanceForTesting(gaia_urls_override.get());

  TestChromeRequestAdapter request(GURL("https://docs.google.com"));
  signin::FixAccountConsistencyRequestHeader(
      &request, GURL(), /*is_off_the_record=*/false,
      /*incognito_availability=*/0, signin::AccountConsistencyMethod::kMirror,
      GaiaId("gaia_id"), signin::ConsentLevel::kSignin,
      /*is_child_account=*/signin::Tribool::kFalse,
      /*is_sync_feature_enabled=*/false,
      CookieSettingsFactory::GetForProfile(profile()).get());
  std::string expected_header =
      "source=Chrome,gaia_origin=example.com,id=gaia_id,mode=0,"
      "enable_account_consistency=true,"
      "supervised=false,consistency_enabled_by_default=false";
  EXPECT_THAT(
      request.modified_headers().GetHeader(signin::kChromeConnectedHeader),
      testing::Optional(expected_header));

  GaiaUrls::SetInstanceForTesting(nullptr);
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kGaiaUrl);
}
#endif  // BUILDFLAG(ENABLE_MIRROR)

TEST_F(ChromeSigninHelperTest,
       ParseGaiaIdFromRemoveLocalAccountResponseHeader) {
  EXPECT_EQ(GaiaId("123456"),
            signin::ParseGaiaIdFromRemoveLocalAccountResponseHeaderForTesting(
                TestResponseAdapter("Google-Accounts-RemoveLocalAccount",
                                    "obfuscatedid=\"123456\"",
                                    /*is_outermost_main_frame=*/false)
                    .GetHeaders()));
  EXPECT_EQ(GaiaId("123456"),
            signin::ParseGaiaIdFromRemoveLocalAccountResponseHeaderForTesting(
                TestResponseAdapter("Google-Accounts-RemoveLocalAccount",
                                    "obfuscatedid=\"123456\",foo=\"bar\"",
                                    /*is_outermost_main_frame=*/false)
                    .GetHeaders()));
  EXPECT_EQ(
      GaiaId(),
      signin::ParseGaiaIdFromRemoveLocalAccountResponseHeaderForTesting(
          TestResponseAdapter("Google-Accounts-RemoveLocalAccount", "malformed",
                              /*is_outermost_main_frame=*/false)
              .GetHeaders()));
}
