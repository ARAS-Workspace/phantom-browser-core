// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include <string.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browser_switcher {

namespace {

const char kAValidUrl[] = "http://example.com/";
const char kAnInvalidUrl[] = "the quick brown fox jumps over the lazy dog";

const char kSitelistXml[] = R"(
  <rules version="1">
    <docMode>
      <domain docMode="9">docs.google.com</domain>
    </docMode>
  </rules>
)";

const char kOtherSitelistXml[] = R"(
  <rules version="1">
    <docMode>
      <domain docMode="9">yahoo.com</domain>
    </docMode>
  </rules>
)";

// This XML parses differently, depending on the value of the
// BrowserSwitcherParsingMode policy.
const char kParsingModeSensitiveSitelistXml[] = R"(
  <site-list version="1">
    <site url="example.com/grey">
      <open-in>None</open-in>
    </site>
    <site url="example.com/chrome">
      <open-in>MSEdge</open-in>
    </site>
    <site url="example.com/ie">
      <open-in>IE11</open-in>
    </site>
  </site-list>
)";

bool ReturnValidXml(content::URLLoaderInterceptor::RequestParams* params) {
  std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
  content::URLLoaderInterceptor::WriteResponse(
      headers, std::string(kSitelistXml), params->client.get());
  return true;
}

bool FailToDownload(content::URLLoaderInterceptor::RequestParams* params) {
  std::string headers = "HTTP/1.1 500 Internal Server Error\n\n";
  content::URLLoaderInterceptor::WriteResponse(headers, "",
                                               params->client.get());
  return true;
}

bool ShouldSwitch(BrowserSwitcherService* service, const GURL& url) {
  return service->sitelist()->ShouldSwitch(url);
}

void SetPolicy(policy::PolicyMap* policies,
               const char* key,
               base::Value value) {
  policies->Set(key, policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                policy::POLICY_SOURCE_PLATFORM, std::move(value), nullptr);
}

void EnableBrowserSwitcher(policy::PolicyMap* policies) {
  SetPolicy(policies, policy::key::kBrowserSwitcherEnabled, base::Value(true));
}

}  // namespace

class BrowserSwitcherServiceTest : public InProcessBrowserTest {
 public:
  ~BrowserSwitcherServiceTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    BrowserSwitcherService::SetRefreshDelayForTesting(base::TimeDelta());
  }

  void SetUpOnMainThread() override {
  }

  void UpdatePolicies(policy::PolicyMap& policies) {
    provider_.UpdateChromePolicy(policies);
    base::RunLoop().RunUntilIdle();
    BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->GetProfile())
        ->Init();
  }

  void SetExternalUrl(const std::string& url) {
    policy::PolicyMap policies;
    EnableBrowserSwitcher(&policies);
    SetPolicy(&policies, policy::key::kBrowserSwitcherExternalSitelistUrl,
              base::Value(url));
    UpdatePolicies(policies);
  }

  void WaitForRefresh() {
    base::RunLoop run_loop;
    GetService()->OnAllRulesetsLoadedForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  void WaitForActionTimeout() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
  }

  BrowserSwitcherService* GetService() {
    return BrowserSwitcherServiceFactory::GetForBrowserContext(
        browser()->GetProfile());
  }

  policy::MockConfigurationPolicyProvider& policy_provider() {
    return provider_;
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

};

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, ExternalSitelistInvalidUrl) {
  SetExternalUrl(kAnInvalidUrl);

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams* params) {
        if (!params->url_request.url.is_valid() ||
            params->url_request.url.spec() == kAnInvalidUrl) {
          *happened = true;
        }
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  GetService();
  WaitForActionTimeout();
  EXPECT_FALSE(fetch_happened);
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalFetchAndParseAfterStartup) {
  SetExternalUrl(kAValidUrl);

  int counter = 0;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](int* counter, content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() != kAValidUrl)
          return false;
        // Return a different sitelist on refresh.
        const char* sitelist_xml =
            (*counter == 0) ? kSitelistXml : kOtherSitelistXml;
        std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
        content::URLLoaderInterceptor::WriteResponse(
            headers, std::string(sitelist_xml), params->client.get());
        (*counter)++;
        return true;
      },
      &counter));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://yahoo.com/")));

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://yahoo.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalFirstFetchFailsButSecondWorks) {
  // Use a non-zero refresh delay so that the second refresh doesn't finish
  // before we've had a chance to check the state after the first (failed)
  // refresh.
  BrowserSwitcherService::SetRefreshDelayForTesting(base::Milliseconds(500));

  SetExternalUrl(kAValidUrl);

  int counter = 0;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](int* counter, content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() != kAValidUrl)
          return false;
        // First request fails, but second succeeds.
        if (*counter == 0)
          FailToDownload(params);
        else
          ReturnValidXml(params);
        (*counter)++;
        return true;
      },
      &counter));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://yahoo.com/")));

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalListensForPrefChanges) {
  // Start with an invalid URL, so no sitelist.
  SetExternalUrl(kAnInvalidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(ReturnValidXml));

  auto* service = GetService();

  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));

  SetExternalUrl(kAValidUrl);
  WaitForRefresh();
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));

  SetExternalUrl(kAnInvalidUrl);
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
}

// Check that changing the BrowserSwitcherParsingMode policy triggers a
// redownload, and parses the XML with the right ParsingMode.
IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalRedownloadOnParsingModeChange) {
  policy::PolicyMap policies;
  EnableBrowserSwitcher(&policies);
  SetPolicy(&policies, policy::key::kBrowserSwitcherExternalSitelistUrl,
            base::Value(kAValidUrl));
  UpdatePolicies(policies);

  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() != kAValidUrl)
          return false;
        std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
        content::URLLoaderInterceptor::WriteResponse(
            headers, kParsingModeSensitiveSitelistXml, params->client.get());
        return true;
      }));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://example.com/grey")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://example.com/chrome")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://example.com/ie")));
  EXPECT_EQ(3u, service->sitelist()->GetExternalSitelist()->sitelist.size());
  EXPECT_EQ(0u, service->sitelist()->GetExternalSitelist()->greylist.size());
  EXPECT_EQ(
      "!//example.com/grey",
      service->sitelist()->GetExternalSitelist()->sitelist[0]->ToString());
  EXPECT_EQ(
      "//example.com/chrome",
      service->sitelist()->GetExternalSitelist()->sitelist[1]->ToString());
  EXPECT_EQ(
      "//example.com/ie",
      service->sitelist()->GetExternalSitelist()->sitelist[2]->ToString());

  SetPolicy(&policies, policy::key::kBrowserSwitcherParsingMode,
            base::Value(static_cast<int>(ParsingMode::kIESiteListMode)));
  UpdatePolicies(policies);

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://example.com/grey")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://example.com/chrome")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://example.com/ie")));
  EXPECT_EQ(2u, service->sitelist()->GetExternalSitelist()->sitelist.size());
  EXPECT_EQ(1u, service->sitelist()->GetExternalSitelist()->greylist.size());
  EXPECT_EQ(
      "!*://example.com/chrome",
      service->sitelist()->GetExternalSitelist()->sitelist[0]->ToString());
  EXPECT_EQ(
      "*://example.com/ie",
      service->sitelist()->GetExternalSitelist()->sitelist[1]->ToString());
  EXPECT_EQ(
      "*://example.com/grey",
      service->sitelist()->GetExternalSitelist()->greylist[0]->ToString());
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, ExternalFileUrl) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath sitelist_path = dir.GetPath().AppendASCII("sitelist.xml");
  base::WriteFile(sitelist_path, kSitelistXml);

  SetExternalUrl(net::FilePathToFileURL(sitelist_path).spec());

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalIgnoresFailedDownload) {
  SetExternalUrl(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(FailToDownload));

  // Execute everything and make sure no rules are applied.
  auto* service = GetService();

  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalIgnoresNonManagedPref) {
  browser()->GetProfile()->GetPrefs()->SetString(prefs::kExternalSitelistUrl,
                                                 kAValidUrl);

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() == kAValidUrl)
          *happened = true;
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  GetService();
  WaitForActionTimeout();
  EXPECT_FALSE(fetch_happened);
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalGreylistFetchAndParseAfterStartup) {
  policy::PolicyMap policies;
  EnableBrowserSwitcher(&policies);
  auto url_list = base::ListValue().Append("*");
  SetPolicy(&policies, policy::key::kBrowserSwitcherUrlList,
            base::Value(std::move(url_list)));
  SetPolicy(&policies, policy::key::kBrowserSwitcherExternalGreylistUrl,
            base::Value(kAValidUrl));
  UpdatePolicies(policies);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(ReturnValidXml));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();
  WaitForRefresh();
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://yahoo.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       PRE_ExternalCachedForBrowserRestart) {
  SetExternalUrl(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(&ReturnValidXml));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();
  WaitForRefresh();
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://yahoo.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest,
                       ExternalCachedForBrowserRestart) {
  SetExternalUrl(kAValidUrl);

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(&ReturnValidXml));

  // Execute everything and make sure the rules are applied correctly.
  auto* service = GetService();
  // No timeout here, since we're checking that the rules get applied *before*
  // downloading.
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(service, GURL("http://docs.google.com/")));
  EXPECT_FALSE(ShouldSwitch(service, GURL("http://yahoo.com/")));
}

}  // namespace browser_switcher
