// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_ui_bindings.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class DevToolsUIBindingsTest : public testing::Test {};

class DevToolsUIBindingsLoadNetworkResourceTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    bindings_ = std::make_unique<DevToolsUIBindings>(web_contents_);
  }

  content::WebContents* web_contents() { return web_contents_; }
  DevToolsUIBindings* bindings() { return bindings_.get(); }

  void CallLoadNetworkResource(const std::string& url,
                               const std::string& headers,
                               int stream_id,
                               DevToolsUIBindings::DispatchCallback callback) {
    bindings_->LoadNetworkResource(std::move(callback), url, headers,
                                   stream_id);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<DevToolsUIBindings> bindings_;
};

class MockDevToolsUIBindingsDelegate : public DevToolsUIBindings::Delegate {
 public:
  explicit MockDevToolsUIBindingsDelegate(
      content::WebContents* inspected_web_contents)
      : inspected_web_contents_(inspected_web_contents) {}

  content::WebContents* GetInspectedWebContents() override {
    return inspected_web_contents_;
  }
  void ActivateWindow() override {}
  void CloseWindow() override {}
  void Inspect(scoped_refptr<content::DevToolsAgentHost> host) override {}
  void SetInspectedPageBounds(const gfx::Rect& rect) override {}
  void InspectElementCompleted() override {}
  void SetIsDocked(bool is_docked) override {}
  void OpenInNewTab(const std::string& url) override {}
  void OpenSearchResultsInNewTab(const std::string& query) override {}
  void SetWhitelistedShortcuts(const std::string& message) override {}
  void SetEyeDropperActive(bool active) override {}
  void OpenNodeFrontend() override {}
  void InspectedContentsClosing() override {}
  void OnLoadCompleted() override {}
  void ReadyForTest() override {}
  void ConnectionReady() override {}
  void SetOpenNewWindowForPopups(bool value) override {}
  infobars::ContentInfoBarManager* GetInfoBarManager() override {
    return nullptr;
  }
  void RenderProcessGone(bool crashed) override {}
  void ShowCertificateViewer(const std::string& cert_chain) override {}
  int GetDockStateForLogging() override { return 0; }
  int GetOpenedByForLogging() override { return 0; }
  int GetClosedByForLogging() override { return 0; }

 private:
  raw_ptr<content::WebContents> inspected_web_contents_;
};

TEST_F(DevToolsUIBindingsLoadNetworkResourceTest,
       AllowsFileSchemeFromRemoteFrontendWithFlag) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kAllowUnsafeDevToolsRemoteFileLoading);

  GURL remote_url("devtools://devtools/remote/serve_rev/@12345/inspector.html");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             remote_url);

  base::RunLoop run_loop;
  base::DictValue result;

  CallLoadNetworkResource(
      "file:///etc/passwd", "", 0,
      base::BindLambdaForTesting([&](const base::Value* value) {
        result = value->GetDict().Clone();
        run_loop.Quit();
      }));
  run_loop.Run();

  auto* msg = result.FindString("messageOverride");
  EXPECT_EQ(msg, nullptr);
  EXPECT_NE(result.FindInt("statusCode"), 403);
}

TEST_F(DevToolsUIBindingsLoadNetworkResourceTest,
       AllowsFileSchemeFromLocalFrontends) {
  std::vector<GURL> local_urls = {
      GURL("devtools://devtools/bundled/devtools_app.html"),
      GURL("devtools://devtools/custom/inspector.html"),
      GURL("devtools://devtools/blank")};

  for (const GURL& local_url : local_urls) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               local_url);

    base::RunLoop run_loop;
    base::DictValue result;

    CallLoadNetworkResource(
        "file:///etc/passwd", "", 0,
        base::BindLambdaForTesting([&](const base::Value* value) {
          result = value->GetDict().Clone();
          run_loop.Quit();
        }));
    run_loop.Run();

    auto* msg = result.FindString("messageOverride");
    EXPECT_EQ(msg, nullptr);
    EXPECT_NE(result.FindInt("statusCode"), 403);
  }
}

TEST_F(DevToolsUIBindingsLoadNetworkResourceTest,
       ClearExtensionsAPIOnNavigatingAway) {
  bindings()->RegisterExtensionsAPIForTesting("http://example.test", "script");
  EXPECT_EQ(bindings()->GetExtensionsAPIForTesting().size(), 1u);

  // Navigate to a valid DevTools URL first.
  GURL devtools_url("devtools://devtools/bundled/devtools_app.html");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             devtools_url);
  EXPECT_EQ(bindings()->GetExtensionsAPIForTesting().size(), 1u);

  // Navigate away to a non-DevTools URL.
  GURL print_url("https://example.test");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             print_url);
  EXPECT_TRUE(bindings()->GetExtensionsAPIForTesting().empty());
}

TEST_F(DevToolsUIBindingsLoadNetworkResourceTest,
       BlocksFileSchemeFromUntrustedFrontends) {
  std::vector<GURL> untrusted_urls = {
      GURL("devtools://devtools/remote/serve_rev/@12345/inspector.html"),
      GURL("https://chrome-devtools-frontend.appspot.com/serve_rev/@12345/"
           "inspector.html"),
      GURL("https://example.com/index.html")};

  // Set up inspected WebContents to be a local file.
  content::WebContents* inspected_web_contents =
      web_contents_factory_.CreateWebContents(profile_.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      inspected_web_contents, GURL("file:///tmp/index.html"));

  auto delegate =
      std::make_unique<MockDevToolsUIBindingsDelegate>(inspected_web_contents);
  bindings()->SetDelegate(delegate.release());

  for (const GURL& untrusted_url : untrusted_urls) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               untrusted_url);

    base::RunLoop run_loop;
    base::DictValue result;

    CallLoadNetworkResource(
        "file:///etc/passwd", "", 0,
        base::BindLambdaForTesting([&](const base::Value* value) {
          result = value->GetDict().Clone();
          run_loop.Quit();
        }));
    run_loop.Run();

    EXPECT_EQ(result.FindInt("statusCode"), 403);
    ASSERT_NE(result.FindString("messageOverride"), nullptr);
    EXPECT_EQ(*result.FindString("messageOverride"),
              "Local file loading is restricted for remote DevTools. Use "
              "--allow-unsafe-devtools-remote-file-loading to enable it.");
  }
}

TEST_F(DevToolsUIBindingsTest, SanitizeFrontendURL) {
  std::vector<std::pair<std::string, std::string>> tests = {
      {"random-string", "devtools://devtools/"},
      {"http://valid.url/but/wrong", "devtools://devtools/but/wrong"},
      {"devtools://wrong-domain/", "devtools://devtools/"},
      {"devtools://devtools/bundled/devtools.html",
       "devtools://devtools/bundled/devtools.html"},
      {"devtools://devtools:1234/bundled/devtools.html#hash",
       "devtools://devtools/bundled/devtools.html#hash"},
      {"devtools://devtools/some/random/path",
       "devtools://devtools/some/random/path"},
      {"devtools://devtools/bundled/devtools.html?debugFrontend=true",
       "devtools://devtools/bundled/devtools.html?debugFrontend=true"},
      {"devtools://devtools/bundled/devtools.html"
       "?some-flag=flag&v8only=true&debugFrontend=a"
       "&another-flag=another-flag&can_dock=false&isSharedWorker=notreally"
       "&remoteFrontend=sure",
       "devtools://devtools/bundled/devtools.html"
       "?v8only=true&debugFrontend=true"
       "&can_dock=true&isSharedWorker=true&remoteFrontend=true"},
      {"devtools://devtools/?ws=any-value-is-fine",
       "devtools://devtools/?ws=any-value-is-fine"},
      {"devtools://devtools/"
       "?service-backend=ws://localhost:9222/services",
       "devtools://devtools/"
       "?service-backend=ws://localhost:9222/services"},
      {"devtools://devtools/?remoteBase="
       "http://example.com:1234/remote-base#hash",
       "devtools://devtools/#hash"},
      {"devtools://devtools/?ws=1%26evil%3dtrue",
       "devtools://devtools/?ws=1%26evil%3dtrue"},
      {"devtools://devtools/?ws=encoded-ok'",
       "devtools://devtools/?ws=encoded-ok%27"},
      {"devtools://devtools/bundled/inspector.html?"
       "&targetType=tab&can_dock=true",
       "devtools://devtools/bundled/inspector.html?"
       "targetType=tab&can_dock=true"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/inspector.html",
       "devtools://devtools/"},
      {"devtools://devtools/"
       "?enabledExperiments=explosionsWhileTyping;newA11yTool",
       "devtools://devtools/"
       "?enabledExperiments=explosionsWhileTyping;newA11yTool"},
      {"devtools://devtools/?enabledExperiments=invalidExperiment$",
       "devtools://devtools/"},
      {"devtools://devtools/?panel=elements",
       "devtools://devtools/?panel=elements"},
      {"devtools://devtools/?panel=network",
       "devtools://devtools/?panel=network"},
      {"devtools://devtools/?panel=console",
       "devtools://devtools/?panel=console"},
      {"devtools://devtools/?panel=sources",
       "devtools://devtools/?panel=sources"},
      {"devtools://devtools/?panel=resources",
       "devtools://devtools/?panel=resources"},
      {"devtools://devtools/?panel=timeline",
       "devtools://devtools/?panel=timeline"},
      {"devtools://devtools/?panel=heap-profiler",
       "devtools://devtools/?panel=heap-profiler"},
      {"devtools://devtools/?panel=lighthouse",
       "devtools://devtools/?panel=lighthouse"},
      {"devtools://devtools/?panel=security",
       "devtools://devtools/?panel=security"},
      {"devtools://devtools/?panel=chrome-recorder",
       "devtools://devtools/?panel=chrome-recorder"},
      {"devtools://devtools/?panel=unsupported", "devtools://devtools/"},
  };

  for (const auto& pair : tests) {
    GURL url = GURL(pair.first);
    url = DevToolsUIBindings::SanitizeFrontendURL(url);
    EXPECT_EQ(pair.second, url.spec());
  }
}

class DevToolsUIBindingsSyncInfoTest : public testing::Test {
 public:
  void SetUp() override {
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext*) {
          return static_cast<std::unique_ptr<KeyedService>>(
              std::make_unique<syncer::TestSyncService>());
        }));
    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(&profile_));
  }

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;

  TestingProfile profile_;
  raw_ptr<syncer::TestSyncService> sync_service_;
};

TEST_F(DevToolsUIBindingsSyncInfoTest, SyncDisabled) {
  sync_service_->SetSignedOut();

  base::DictValue info =
      DevToolsUIBindings::GetSyncInformationForProfile(&profile_);

  EXPECT_EQ(
      base::FeatureList::IsEnabled(switches::kEnablePreferencesAccountStorage),
      info.FindBool("isSyncActive").value());
}

TEST_F(DevToolsUIBindingsSyncInfoTest, PreferencesNotSynced) {
  sync_service_->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kBookmarks});

  base::DictValue info =
      DevToolsUIBindings::GetSyncInformationForProfile(&profile_);

  EXPECT_THAT(info.FindBool("isSyncActive"), testing::Optional(true));
  EXPECT_THAT(info.FindBool("arePreferencesSynced"), testing::Optional(false));
}

TEST_F(DevToolsUIBindingsSyncInfoTest, ImageAlwaysProvided) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "sync@devtools.dev", signin::ConsentLevel::kSignin);
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin, account_info);

  EXPECT_FALSE(account_info.GetAvatarImage().has_value());

  base::DictValue info =
      DevToolsUIBindings::GetSyncInformationForProfile(&profile_);

  EXPECT_EQ(*info.FindString("accountEmail"), "sync@devtools.dev");
  EXPECT_NE(info.FindString("accountImage"), nullptr);
}

class DevToolsUIBindingsHostConfigTest : public testing::Test {
 public:
  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override { profile_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(DevToolsUIBindingsHostConfigTest, GetHostConfigBasic) {
  base::DictValue result =
      DevToolsUIBindings::GetHostConfigDictionary(profile_.get());

  // Check some basic keys that should always be present.
  EXPECT_TRUE(result.FindDict("devToolsProtocolMonitor"));
  EXPECT_TRUE(result.FindDict("devToolsFlexibleLayout"));

  // The AI and Google Developer Program keys are not present.
  EXPECT_FALSE(result.FindDict("aidaAvailability"));
  EXPECT_FALSE(result.FindDict("devToolsConsoleInsights"));
  EXPECT_FALSE(result.FindDict("devToolsGdpProfilesAvailability"));
}

TEST_F(DevToolsUIBindingsHostConfigTest, GetHostConfigWithFeatures) {
  // Verify initial state of features.
  base::DictValue initial_config =
      DevToolsUIBindings::GetHostConfigDictionary(profile_.get());

  const base::DictValue* initial_protocol_monitor =
      initial_config.FindDict("devToolsProtocolMonitor");
  ASSERT_TRUE(initial_protocol_monitor);
  EXPECT_FALSE(initial_protocol_monitor->FindBool("enabled").value_or(true));

  const base::DictValue* initial_instrumentation_breakpoints =
      initial_config.FindDict("devToolsInstrumentationBreakpoints");
  ASSERT_TRUE(initial_instrumentation_breakpoints);
  EXPECT_FALSE(
      initial_instrumentation_breakpoints->FindBool("enabled").value_or(true));

  const base::DictValue* initial_source_map_scopes =
      initial_config.FindDict("devToolsSourceMapScopesInSourcesPanel");
  ASSERT_TRUE(initial_source_map_scopes);
  EXPECT_FALSE(
      initial_source_map_scopes->FindBool("enabled").value_or(true));

  // Enable features.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {::features::kDevToolsProtocolMonitor,
       ::features::kDevToolsInstrumentationBreakpoints,
       ::features::kDevToolsSourceMapScopesInSourcesPanel},
      {});

  // Verify state of features after enabling them.
  base::DictValue result =
      DevToolsUIBindings::GetHostConfigDictionary(profile_.get());

  const base::DictValue* protocol_monitor =
      result.FindDict("devToolsProtocolMonitor");
  ASSERT_TRUE(protocol_monitor);
  EXPECT_TRUE(protocol_monitor->FindBool("enabled").value_or(false));

  const base::DictValue* instrumentation_breakpoints =
      result.FindDict("devToolsInstrumentationBreakpoints");
  ASSERT_TRUE(instrumentation_breakpoints);
  EXPECT_TRUE(
      instrumentation_breakpoints->FindBool("enabled").value_or(false));

  const base::DictValue* source_map_scopes =
      result.FindDict("devToolsSourceMapScopesInSourcesPanel");
  ASSERT_TRUE(source_map_scopes);
  EXPECT_TRUE(source_map_scopes->FindBool("enabled").value_or(false));
}
