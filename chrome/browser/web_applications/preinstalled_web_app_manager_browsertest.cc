// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"

#include <string_view>

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/extensions/chrome_app_deprecation.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/ssl_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_result.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_app_config_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_page_waiter.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "net/ssl/ssl_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/codec/png_codec.h"

namespace web_app {

namespace {

constexpr char kBaseDataDir[] = "chrome/test/data/banners";

// start_url in manifest.json matches navigation url for the simple
// manifest_test_page.html.
constexpr char kSimpleManifestStartUrl[] =
    "https://example.org/manifest_test_page.html";

constexpr char kNoManifestTestPageStartUrl[] =
    "https://example.org/no_manifest_test_page.html";

// Performs blocking IO operations.
base::FilePath GetDataFilePath(const base::FilePath& relative_path,
                               bool* path_exists) {
  base::ScopedAllowBlockingForTesting allow_io;

  base::FilePath root_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path));
  base::FilePath path = root_path.Append(relative_path);
  *path_exists = base::PathExists(path);
  return path;
}

}  // namespace

class PreinstalledWebAppManagerBrowserTestBase
    : public extensions::ExtensionBrowserTest {
 public:
  PreinstalledWebAppManagerBrowserTestBase()
      : skip_preinstalled_web_app_startup_(
            PreinstalledWebAppManager::SkipStartupForTesting()) {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    web_app::test::WaitUntilReady(
        WebAppProvider::GetForTest(browser()->GetProfile()));
  }

  void TearDownOnMainThread() override {
    ResetInterceptor();
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  void InitUrlLoaderInterceptor() {
    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // a stable app_id across tests requires stable origin, whereas
    // EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) -> bool {
              std::string relative_request =
                  base::StrCat({kBaseDataDir, params->url_request.url.path()});
              base::FilePath relative_path =
                  base::FilePath().AppendASCII(relative_request);

              bool path_exists = false;
              base::FilePath path =
                  GetDataFilePath(relative_path, &path_exists);
              if (!path_exists)
                return /*intercepted=*/false;

              // Provide fake SSLInfo to avoid NOT_FROM_SECURE_ORIGIN error in
              // InstallableManager::GetData().
              net::SSLInfo ssl_info;
              CreateFakeSslInfoCertificate(&ssl_info);

              content::URLLoaderInterceptor::WriteResponse(
                  path, params->client.get(), /*headers=*/nullptr, ssl_info,
                  params->url_request.url);

              return /*intercepted=*/true;
            }));
  }

  GURL GetAppUrl() const {
    return embedded_test_server()->GetURL("/web_apps/basic.html");
  }

  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }

  WebAppIconManager& icon_manager() { return provider().icon_manager(); }

  PreinstalledWebAppManager& manager() {
    return provider().preinstalled_web_app_manager();
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  void SyncEmptyConfigs() {
    base::ListValue app_configs;
    base::AutoReset<const base::ListValue*> configs_for_testing =
        PreinstalledWebAppManager::SetConfigsForTesting(&app_configs);

    base::RunLoop run_loop;
    WebAppProvider::GetForTest(profile())
        ->preinstalled_web_app_manager()
        .LoadAndSynchronizeForTesting(base::BindLambdaForTesting(
            [&](std::map<GURL, ExternallyManagedAppManager::InstallResult>
                    install_results,
                std::map<GURL, webapps::UninstallResultCode>
                    uninstall_results) {
              EXPECT_EQ(install_results.size(), 0u);
              EXPECT_EQ(uninstall_results.size(), 0u);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Mocks "icon.png" as chrome/test/data/web_apps/blue-192.png.
  std::optional<webapps::InstallResultCode> SyncPreinstalledAppConfig(
      const GURL& install_url,
      std::string_view app_config_string) {
    base::FilePath test_config_dir(FILE_PATH_LITERAL("test_dir"));
    auto config_auto_reset =
        test::SetPreinstalledWebAppConfigDirForTesting(test_config_dir);

    base::FilePath source_root_dir;
    CHECK(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir));
    base::FilePath test_icon_path =
        source_root_dir.Append(GetChromeTestDataDir())
            .AppendASCII("web_apps/blue-192.png");
    scoped_refptr<TestFileUtils> file_utils = TestFileUtils::Create(
        {{base::FilePath(FILE_PATH_LITERAL("test_dir/icon.png")),
          test_icon_path}});
    base::AutoReset<FileUtilsWrapper*> file_utils_for_testing =
        PreinstalledWebAppManager::SetFileUtilsForTesting(file_utils.get());

    base::ListValue app_configs;
    auto json_parse_result = base::JSONReader::ReadAndReturnValueWithError(
        app_config_string, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    EXPECT_TRUE(json_parse_result.has_value())
        << "JSON parse error: " << json_parse_result.error().message;
    if (!json_parse_result.has_value())
      return std::nullopt;
    app_configs.Append(std::move(*json_parse_result));
    base::AutoReset<const base::ListValue*> configs_for_testing =
        PreinstalledWebAppManager::SetConfigsForTesting(&app_configs);

    std::optional<webapps::InstallResultCode> code;
    base::RunLoop sync_run_loop;
    WebAppProvider::GetForTest(profile())
        ->preinstalled_web_app_manager()
        .LoadAndSynchronizeForTesting(base::BindLambdaForTesting(
            [&](std::map<GURL, ExternallyManagedAppManager::InstallResult>
                    install_results,
                std::map<GURL, webapps::UninstallResultCode>
                    uninstall_results) {
              auto it = install_results.find(install_url);
              if (it != install_results.end())
                code = it->second.code;
              sync_run_loop.Quit();
            }));
    sync_run_loop.Run();

    return code;
  }

  struct SyncResults {
    std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results;
    std::map<GURL, webapps::UninstallResultCode> uninstall_results;
  };
  SyncResults SyncPreinstalledApps() {
    base::test::TestFuture<
        std::map<GURL, ExternallyManagedAppManager::InstallResult>,
        std::map<GURL, webapps::UninstallResultCode>>
        future;

    WebAppProvider::GetForTest(profile())
        ->preinstalled_web_app_manager()
        .LoadAndSynchronizeForTesting(future.GetCallback());
    return {
        .install_results = future.Get<
            std::map<GURL, ExternallyManagedAppManager::InstallResult>>(),
        .uninstall_results =
            future.Get<std::map<GURL, webapps::UninstallResultCode>>(),
    };
  }

  ~PreinstalledWebAppManagerBrowserTestBase() override = default;

 protected:
  void ResetInterceptor() { url_loader_interceptor_.reset(); }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  base::AutoReset<bool> skip_preinstalled_web_app_startup_;
};

class PreinstalledWebAppManagerBrowserTest
    : public PreinstalledWebAppManagerBrowserTestBase {
 public:
  PreinstalledWebAppManagerBrowserTest() = default;

 private:
  base::test::ScopedFeatureList feature_list_{features::kRecordWebAppDebugInfo};
};

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       LaunchQueryParamsBasic) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  EXPECT_FALSE(registrar().GetInstallState(app_id).has_value());

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "launch_query_params": "test_launch_params"
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {start_url.spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(start_url, app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_EQ(registrar().GetInstallState(app_id),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
  );
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);

  GURL launch_url =
      embedded_test_server()->GetURL("/web_apps/basic.html?test_launch_params");
  EXPECT_EQ(registrar().GetAppLaunchUrl(app_id), launch_url);

  // This is required to allow launching to work.
  provider().scheduler().SynchronizeOsIntegration(app_id, base::DoNothing());

  Browser* app_browser = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      launch_url);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       LaunchQueryParamsDuplicate) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL install_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html");
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html?query_params=in&start=url");
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  EXPECT_FALSE(registrar().GetInstallState(app_id).has_value());

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "launch_query_params": "query_params=in"
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {install_url.spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(install_url, app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_EQ(registrar().GetInstallState(app_id),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
  );
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);

  // We should not duplicate the query param if start_url already has it.
  EXPECT_EQ(registrar().GetAppLaunchUrl(app_id), start_url);

  // This is required to allow launching to work.
  provider().scheduler().SynchronizeOsIntegration(app_id, base::DoNothing());

  Browser* app_browser = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      start_url);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       LaunchQueryParamsMultiple) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  GURL launch_url = embedded_test_server()->GetURL(
      "/web_apps/basic.html?more=than&one=query&param");
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  EXPECT_FALSE(registrar().GetInstallState(app_id).has_value());

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "launch_query_params": "more=than&one=query&param"
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {start_url.spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(start_url, app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_EQ(registrar().GetInstallState(app_id),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
  );
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);
  EXPECT_EQ(registrar().GetAppLaunchUrl(app_id), launch_url);

  // This is required to allow launching to work.
  provider().scheduler().SynchronizeOsIntegration(app_id, base::DoNothing());

  Browser* app_browser = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      launch_url);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       LaunchQueryParamsComplex) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL install_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html");
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html?query_params=in&start=url");
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  EXPECT_FALSE(registrar().GetInstallState(app_id).has_value());

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "launch_query_params": "!@#$$%^*&)("
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {install_url.spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(install_url, app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_EQ(registrar().GetInstallState(app_id),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
  );
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);

  GURL launch_url = embedded_test_server()->GetURL(
      "/web_apps/"
      "query_params_in_start_url.html?query_params=in&start=url&!@%23$%^*&)(");
  EXPECT_EQ(registrar().GetAppLaunchUrl(app_id), launch_url);

  provider().scheduler().SynchronizeOsIntegration(app_id, base::DoNothing());

  Browser* app_browser = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      launch_url);
}

class PreinstalledWebAppManagerExtensionBrowserTest
    : public PreinstalledWebAppManagerBrowserTest {
 public:
  PreinstalledWebAppManagerExtensionBrowserTest()
      : enable_chrome_apps_(
            &extensions::testing::g_enable_chrome_apps_for_testing,
            true) {}
  ~PreinstalledWebAppManagerExtensionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PreinstalledWebAppManagerBrowserTest::SetUpOnMainThread();
    web_app::test::WaitUntilReady(
        WebAppProvider::GetForTest(browser()->GetProfile()));
  }
  void TearDownOnMainThread() override {
    ResetInterceptor();
    PreinstalledWebAppManagerBrowserTest::TearDownOnMainThread();
  }

 private:
  base::AutoReset<bool> enable_chrome_apps_;
};

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerExtensionBrowserTest,
                       UninstallAndReplace) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install Chrome app to be replaced.
  const char kChromeAppDirectory[] = "app";
  const char kChromeAppName[] = "App Test";
  const extensions::Extension* app = InstallExtensionWithSourceAndFlags(
      test_data_dir_.AppendASCII(kChromeAppDirectory), 1,
      extensions::mojom::ManifestLocation::kInternal,
      extensions::Extension::NO_FLAGS);
  EXPECT_EQ(app->name(), kChromeAppName);

  // Start listening for Chrome app uninstall.
  extensions::TestExtensionRegistryObserver uninstall_observer(
      extensions::ExtensionRegistry::Get(browser()->GetProfile()));

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"],
        "uninstall_and_replace": ["$2"]
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {GetAppUrl().spec(), app->id()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  // Chrome app should get uninstalled.
  scoped_refptr<const extensions::Extension> uninstalled_app =
      uninstall_observer.WaitForExtensionUninstalled();
  EXPECT_EQ(app, uninstalled_app.get());
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PreinstalledAppsPrefInstall) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());
  // This uses an "extensions" pref for historical reasons.
  profile()->GetPrefs()->SetString(prefs::kPreinstalledExtensions, "install");

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {GetAppUrl().spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
            webapps::InstallResultCode::kSuccessNewInstall);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PreinstalledAppsPrefNoinstall) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());
  // This uses an "extensions" pref for historical reasons.
  profile()->GetPrefs()->SetString(prefs::kPreinstalledExtensions, "noinstall");

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {GetAppUrl().spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config), std::nullopt);
}

const char kOnlyIfPreviouslyPreinstalled_PreviousConfig[] = R"({
  "app_url": "$1",
  "launch_container": "window",
  "user_type": ["unmanaged"]
})";
const char kOnlyIfPreviouslyPreinstalled_NextConfig[] = R"({
  "app_url": "$1",
  "launch_container": "window",
  "user_type": ["unmanaged"],
  "only_if_previously_preinstalled": true
})";

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PRE_OnlyIfPreviouslyPreinstalled_AppPreserved) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  InitUrlLoaderInterceptor();

  std::string prev_app_config = base::ReplaceStringPlaceholders(
      kOnlyIfPreviouslyPreinstalled_PreviousConfig, {kSimpleManifestStartUrl},
      nullptr);

  // The user had the app installed.
  EXPECT_EQ(
      SyncPreinstalledAppConfig(GURL{kSimpleManifestStartUrl}, prev_app_config),
      webapps::InstallResultCode::kSuccessNewInstall);

  webapps::AppId app_id = GenerateAppId(/*manifest_id=*/std::nullopt,
                                        GURL{kSimpleManifestStartUrl});
  EXPECT_EQ(registrar().GetInstallState(app_id),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
  );
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       OnlyIfPreviouslyPreinstalled_AppPreserved) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  InitUrlLoaderInterceptor();

  std::string next_app_config =
      base::ReplaceStringPlaceholders(kOnlyIfPreviouslyPreinstalled_NextConfig,
                                      {kSimpleManifestStartUrl}, nullptr);

  // The user still has the app.
  EXPECT_EQ(
      SyncPreinstalledAppConfig(GURL{kSimpleManifestStartUrl}, next_app_config),
      webapps::InstallResultCode::kSuccessAlreadyInstalled);

  webapps::AppId app_id = GenerateAppId(/*manifest_id=*/std::nullopt,
                                        GURL{kSimpleManifestStartUrl});
  EXPECT_EQ(registrar().GetInstallState(app_id),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
  );
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PRE_OnlyIfPreviouslyPreinstalled_NoAppPreinstalled) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  InitUrlLoaderInterceptor();

  std::string prev_app_config = base::ReplaceStringPlaceholders(
      kOnlyIfPreviouslyPreinstalled_PreviousConfig,
      {kNoManifestTestPageStartUrl}, nullptr);

  EXPECT_EQ(SyncPreinstalledAppConfig(GURL{kNoManifestTestPageStartUrl},
                                      prev_app_config),
            webapps::InstallResultCode::kNotValidManifestForWebApp);

  webapps::AppId app_id = GenerateAppId(/*manifest_id=*/std::nullopt,
                                        GURL{kNoManifestTestPageStartUrl});
  EXPECT_FALSE(registrar().GetInstallState(app_id).has_value());
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       OnlyIfPreviouslyPreinstalled_NoAppPreinstalled) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  InitUrlLoaderInterceptor();

  std::string next_app_config =
      base::ReplaceStringPlaceholders(kOnlyIfPreviouslyPreinstalled_NextConfig,
                                      {kNoManifestTestPageStartUrl}, nullptr);

  // The user has no the app.
  EXPECT_EQ(SyncPreinstalledAppConfig(GURL{kNoManifestTestPageStartUrl},
                                      next_app_config),
            std::nullopt);

  webapps::AppId app_id = GenerateAppId(/*manifest_id=*/std::nullopt,
                                        GURL{kNoManifestTestPageStartUrl});
  EXPECT_FALSE(registrar().GetInstallState(app_id).has_value());
}

const char kFeatureNameOrInstalledConfig[] = R"({
  "app_url": "$1",
  "launch_container": "window",
  "user_type": ["unmanaged"],
  "feature_name_or_installed": "test_feature"
})";

// When the "feature_name_or_installed" feature is enabled, the app should be
// installed.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       GateOnFeatureNameOrInstalled_InstallWhenEnabled) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  base::AutoReset<bool> enable_feature =
      SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting();

  std::string app_config = base::ReplaceStringPlaceholders(
      kFeatureNameOrInstalledConfig, {GetAppUrl().spec()}, nullptr);

  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());
  EXPECT_EQ(registrar().GetInstallState(app_id),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
  );
}

// When the "feature_name_or_installed" feature is disabled, the app should not
// be installed.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       GateOnFeatureNameOrInstalled_IgnoreWhenDisabled) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string app_config = base::ReplaceStringPlaceholders(
      kFeatureNameOrInstalledConfig, {GetAppUrl().spec()}, nullptr);

  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config), std::nullopt);

  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());
  EXPECT_FALSE(registrar().GetInstallState(app_id).has_value());
}

// When the "feature_name_or_installed" feature is disabled, any existing
// preinstalled app should not be uninstalled.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       Installed_DoNotUninstallWhenDisabled) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string app_config = base::ReplaceStringPlaceholders(
      kFeatureNameOrInstalledConfig, {GetAppUrl().spec()}, nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());

  {
    base::AutoReset<bool> enable_feature =
        SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting();
    EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
              webapps::InstallResultCode::kSuccessNewInstall);

    EXPECT_EQ(registrar().GetInstallState(app_id),
              proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
    );
  }

  {
    EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
              webapps::InstallResultCode::kSuccessAlreadyInstalled);

    EXPECT_EQ(registrar().GetInstallState(app_id),
              proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
    );
  }
}

// Preinstalled apps which are user uninstalled become ignored configs.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       DisableForPreinstalledAppsInConfig) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string config = base::ReplaceStringPlaceholders(
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })",
      {GetAppUrl().spec()}, nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());

  // Preinstall web app.
  {
    base::HistogramTester tester;
    EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), config),
              webapps::InstallResultCode::kSuccessNewInstall);
    EXPECT_EQ(registrar().GetInstallState(app_id),
              proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
    );
    tester.ExpectUniqueSample("WebApp.Preinstalled.DisabledReason",
                              /*kNotDisabled*/ 0, 1);
  }

  // Mark web app as user uninstalled without uninstalling it.
  // This is an erroneous state that users have gotten into in the past via
  // database migration bugs, see crbug.com/40880824 and crbug.com/1363004 for
  // past incidents.
  {
    UserUninstalledPreinstalledWebAppPrefs prefs(profile()->GetPrefs());
    prefs.Add(app_id, {GetAppUrl()});
    ASSERT_EQ(app_id, prefs.LookUpAppIdByInstallUrl(GetAppUrl()));
  }

  // Check web app does not get uninstalled by PWAM sync.
  {
    base::HistogramTester tester;
    EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), config),
              webapps::InstallResultCode::kSuccessAlreadyInstalled);
    EXPECT_EQ(registrar().GetInstallState(app_id),
              proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
    );
    tester.ExpectUniqueSample("WebApp.Preinstalled.DisabledReason",
                              /*kIgnorePreviouslyUninstalledByUser*/ 17, 1);
  }

  // Actually uninstall web app.
  {
    base::test::TestFuture<webapps::UninstallResultCode> future;
    provider().scheduler().RemoveUserUninstallableManagements(
        app_id, webapps::WebappUninstallSource::kAppMenu, future.GetCallback());
    ASSERT_EQ(future.Get(), webapps::UninstallResultCode::kAppRemoved);
    ASSERT_FALSE(registrar().GetInstallState(app_id).has_value());
  }

  // Check web app does not get installed by PWAM sync.
  {
    base::HistogramTester tester;
    EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), config), std::nullopt);
    EXPECT_FALSE(registrar().GetInstallState(app_id).has_value());
    tester.ExpectUniqueSample("WebApp.Preinstalled.DisabledReason",
                              /*kIgnorePreviouslyUninstalledByUser*/ 17, 1);
  }
}

// Preinstalled apps which are user uninstalled are included
// in the config passed to the ExternallyManagedAppInstallManager if
// |override_previous_user_uninstall| is true.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       PreinstalledAppsUninstallOverride) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  base::AutoReset<bool> override_previous_user_uninstall_config =
      PreinstalledWebAppManager::
          OverridePreviousUserUninstallConfigForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  const auto manifest = base::ReplaceStringPlaceholders(
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })",
      {GetAppUrl().spec()}, nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());
  ASSERT_FALSE(registrar().GetInstallState(app_id).has_value());
  UserUninstalledPreinstalledWebAppPrefs prefs(profile()->GetPrefs());
  prefs.Add(app_id, {GetAppUrl()});

  // Verify prefs have the proper data.
  EXPECT_EQ(1, prefs.Size());
  EXPECT_EQ(app_id, prefs.LookUpAppIdByInstallUrl(GetAppUrl()));

  // On sync across configs, app is installed because
  // |override_previous_user_uninstall| is true.
  const auto& ignore_configs = manager().debug_info()->ignore_configs;
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), manifest),
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_EQ(registrar().GetInstallState(app_id),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
  );
  EXPECT_EQ(ignore_configs.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerBrowserTest,
                       IgnoreCorruptUserUninstalledPreinstalledWebAppPrefs) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {GetAppUrl().spec()}, nullptr);
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
            webapps::InstallResultCode::kSuccessNewInstall);

  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());
  EXPECT_TRUE(registrar().AppMatches(
      app_id, WebAppFilter::InstalledByDefaultManagement()));

  // Simulate the effects of https://crbug.com/40237276 by adding an installed
  // preinstalled web app to the "has been uninstalled by the user" pref even
  // though the web app is still kDefault installed.
  UserUninstalledPreinstalledWebAppPrefs(profile()->GetPrefs())
      .Add(app_id, {GetAppUrl()});

  // Check that the PreinstalledWebAppManager doesn't uninstall the web app
  // just because the prefs say it's uninstalled.
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), app_config),
            webapps::InstallResultCode::kSuccessAlreadyInstalled);
  EXPECT_TRUE(registrar().AppMatches(
      app_id, WebAppFilter::InstalledByDefaultManagement()));
}

// The offline manifest JSON config functionality is only available on Chrome
// OS.

class PreinstalledWebAppManagerPreferredAppForSupportedLinksBrowserTest
    : public PreinstalledWebAppManagerBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple</*is_preferred_app_for_supported_links=*/bool,
                     apps::test::LinkCapturingFeatureVersion>> {
 public:
  PreinstalledWebAppManagerPreferredAppForSupportedLinksBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(
            std::get<apps::test::LinkCapturingFeatureVersion>(GetParam())),
        {});
  }

  bool IsPreferredAppPerInstallOption() const {
    return std::get<0>(GetParam());
  }

  bool AppsCapturingByDefault() const {
    return std::get<apps::test::LinkCapturingFeatureVersion>(GetParam()) ==
           apps::test::LinkCapturingFeatureVersion::kV2DefaultOn;
  }

  bool GetExpectedPreferredAppForSupportedLinks() const {
    return IsPreferredAppPerInstallOption();
  }

  void RemoveSupportedLinksPreference(const webapps::AppId& app_id) {
    apps_util::RemoveSupportedLinksPreferenceAndWait(profile(), app_id);
  }

  void WaitForSupportedLinksPreference(const webapps::AppId& app_id,
                                       bool is_preferred_app) {
    apps_util::PreferredAppUpdateWaiter(
        apps::AppServiceProxyFactory::GetForProfile(profile())
            ->PreferredAppsList(),
        app_id, is_preferred_app)
        .Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    PreinstalledWebAppManagerPreferredAppForSupportedLinksBrowserTest,
    MaybeSetPreferredAppForSupportedLinks) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  const auto manifest = base::ReplaceStringPlaceholders(
      R"({
        "app_url": "$1",
        "is_preferred_app_for_supported_links": $2,
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })",
      {GetAppUrl().spec(), base::ToString(IsPreferredAppPerInstallOption())},
      nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, GetAppUrl());

  // Install the app for the first time.
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), manifest),
            webapps::InstallResultCode::kSuccessNewInstall);
  apps::AppReadinessWaiter(profile(), app_id).Await();
  EXPECT_EQ(registrar().GetInstallState(app_id),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
  );

  // Verify that the app is the preferred app if requested in the install
  // options, or if v2DefaultOn is enabled.
  WaitForSupportedLinksPreference(app_id,
                                  GetExpectedPreferredAppForSupportedLinks());

  // Clear the preferred app.
  RemoveSupportedLinksPreference(app_id);

  // Reinstall the app.
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), manifest),
            webapps::InstallResultCode::kSuccessAlreadyInstalled);
  EXPECT_EQ(registrar().GetInstallState(app_id),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
  );

  // Verify that the app is *not* the preferred app after re-installation as
  // the user may have already updated their preference.
  WaitForSupportedLinksPreference(app_id, /*is_preferred_app=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PreinstalledWebAppManagerPreferredAppForSupportedLinksBrowserTest,
    testing::Combine(
        /*is_preferred_app_for_supported_links=*/testing::Bool(),
        testing::Values(
            apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
            apps::test::LinkCapturingFeatureVersion::kV2DefaultOn)));

// State denoting whether preinstalled apps are capturing links by default or
// not based on the `kPreinstalledBrowserTabWebAppsCaptureOnDefault` flag.
enum class PreinstalledAppCaptureState {
  kForcedOn,
  kForcedOff,
};

// State denoting whether the safety flag to prevent preinstalled apps from
// capturing is enabled or not.
enum class PreinstalledAppSafetyFlagStatus {
  kSwitchedOn,
  kSwitchedOff,
};

class PreinstalledWebAppNavigationCapturing
    : public PreinstalledWebAppManagerBrowserTest,
      public testing::WithParamInterface<
          std::tuple<PreinstalledAppCaptureState,
                     PreinstalledAppSafetyFlagStatus,
                     apps::test::LinkCapturingFeatureVersion>> {
 public:
  PreinstalledWebAppNavigationCapturing() {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        apps::test::GetFeaturesToEnableLinkCapturingUX(
            std::get<apps::test::LinkCapturingFeatureVersion>(GetParam()));
    std::vector<base::test::FeatureRef> disabled_features;

    // Setup whether preinstalled apps should capture links by default.
    if (ArePreinstalledAppsCapturingByDefault()) {
      enabled_features.emplace_back(
          kPreinstalledBrowserTabWebAppsCaptureOnDefault,
          base::FieldTrialParams());
    } else {
      disabled_features.emplace_back(
          kPreinstalledBrowserTabWebAppsCaptureOnDefault);
    }

    // Setup whether the safety flag to prevent preinstalled apps from capturing
    // links has been set.
    if (ShouldForceStopPreinstalledAppsCapture()) {
      enabled_features.emplace_back(
          kPreinstalledBrowserTabWebAppsForcedDefaultCaptureOff,
          base::FieldTrialParams());
    } else {
      disabled_features.emplace_back(
          kPreinstalledBrowserTabWebAppsForcedDefaultCaptureOff);
    }

    nav_capturing_on_.InitWithFeaturesAndParameters(enabled_features,
                                                    disabled_features);
  }

  bool ArePreinstalledAppsCapturingByDefault() {
    return std::get<PreinstalledAppCaptureState>(GetParam()) ==
           PreinstalledAppCaptureState::kForcedOn;
  }

  bool ShouldForceStopPreinstalledAppsCapture() {
    return std::get<PreinstalledAppSafetyFlagStatus>(GetParam()) ==
           PreinstalledAppSafetyFlagStatus::kSwitchedOn;
  }

  bool ShouldCaptureLinksByDefault() {
    return std::get<apps::test::LinkCapturingFeatureVersion>(GetParam()) ==
           apps::test::LinkCapturingFeatureVersion::kV2DefaultOn;
  }

  // Capture links for preinstalled apps iff:
  // 1. The safety flag `kPreinstalledBrowserTabWebAppsForcedDefaultCaptureOff`
  // is not set.
  // 2. If either the whole reimplementation version is set to capture links by
  // default, or in the absence of that,
  // `kPreinstalledBrowserTabWebAppsCaptureOnDefault` is set only for
  // preinstalled apps.
  bool ShouldCaptureLinks() {
    return !ShouldForceStopPreinstalledAppsCapture() &&
           (ArePreinstalledAppsCapturingByDefault() ||
            ShouldCaptureLinksByDefault());
  }

 private:
  base::test::ScopedFeatureList nav_capturing_on_;
};

IN_PROC_BROWSER_TEST_P(PreinstalledWebAppNavigationCapturing,
                       PreinstalledAppsCaptureLinks) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  ASSERT_TRUE(embedded_test_server()->Start());

  const auto manifest = base::ReplaceStringPlaceholders(
      R"({
        "app_url": "$1",
        "is_preferred_app_for_supported_links": false,
        "launch_container": "tab",
        "user_type": ["unmanaged"]
      })",
      {GetAppUrl().spec()}, nullptr);
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, GetAppUrl());

  // Install the app for the first time.
  EXPECT_EQ(SyncPreinstalledAppConfig(GetAppUrl(), manifest),
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_EQ(registrar().GetInstallState(app_id),
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION
  );

  EXPECT_EQ(ShouldCaptureLinks(), registrar().CapturesLinksInScope(app_id));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PreinstalledWebAppNavigationCapturing,
    testing::Combine(
        testing::Values(PreinstalledAppCaptureState::kForcedOn,
                        PreinstalledAppCaptureState::kForcedOff),
        testing::Values(PreinstalledAppSafetyFlagStatus::kSwitchedOn,
                        PreinstalledAppSafetyFlagStatus::kSwitchedOff),
        testing::Values(apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                        apps::test::LinkCapturingFeatureVersion::kV2DefaultOn)),
    [](const auto& param_info) {
      std::string test_name;
      test_name.append(apps::test::ToString(
          std::get<apps::test::LinkCapturingFeatureVersion>(param_info.param)));
      test_name.append("_");
      switch (std::get<PreinstalledAppCaptureState>(param_info.param)) {
        case PreinstalledAppCaptureState::kForcedOn:
          test_name.append("PreinstalledCaptureOn");
          break;
        case PreinstalledAppCaptureState::kForcedOff:
          test_name.append("PreinstalledCaptureOff");
          break;
      }
      test_name.append("_");
      switch (std::get<PreinstalledAppSafetyFlagStatus>(param_info.param)) {
        case PreinstalledAppSafetyFlagStatus::kSwitchedOn:
          test_name.append("SafetyFlagSwitchedOn");
          break;
        case PreinstalledAppSafetyFlagStatus::kSwitchedOff:
          test_name.append("SafetyFlagSwitchedOff");
          break;
      }
      return test_name;
    });

class PreinstalledWebAppManagerSimpleBrowserTest
    : public WebAppBrowserTestBase {
 public:
  static constexpr std::string_view kHostname = "www.example.com";
  static constexpr std::string_view kStartUrl = "/web_apps/simple/index.html";
  static constexpr std::string_view kScope = "/web_apps/simple/";
  static constexpr std::string_view kInstallUrl =
      "/web_apps/simple/install_url.html";
  static constexpr std::string_view kManifestId = "/web_app/simple/index.html";
  static constexpr std::string_view kManifestUrl =
      "/web_app/simple/manifest.json";
  static constexpr base::FilePath::StringViewType kIcon48 =
      FILE_PATH_LITERAL("web_apps/simple/basic-48.png");
  static constexpr base::FilePath::StringViewType kIcon192 =
      FILE_PATH_LITERAL("web_apps/simple/basic-192.png");
  static constexpr std::u16string_view kWrongName = u"Wrong App Name";

  // Just a page that is out of scope.
  static constexpr std::string_view kOutOfScopeUrl =
      "/web_apps/install_url/index.html";

  PreinstalledWebAppManagerSimpleBrowserTest() {
    fake_provider_creator_ =
        std::make_unique<FakeWebAppProviderCreator>(base::BindRepeating(
            [](base::WeakPtr<PreinstalledWebAppManagerSimpleBrowserTest> test,
               Profile* profile) -> std::unique_ptr<KeyedService> {
              if (!test) {
                return nullptr;
              }
              std::unique_ptr<WebAppProvider> provider =
                  std::make_unique<WebAppProvider>(profile);
              test->run_delayed_startup_tasks_ =
                  provider->DisableDelayedPostStartupWorkForTesting();
              provider->preinstalled_web_app_manager()
                  .SetPreinstalledAppForUpdatingForTesting(
                      PreinstalledAppForUpdating{test->GetManifestId(),
                                                 test->GetInstallUrl()});
              provider->Start();
              return provider;
            },
            weak_ptr_factory_.GetWeakPtr()));
  }
  ~PreinstalledWebAppManagerSimpleBrowserTest() override = default;

  GURL GetStartUrl() {
    return embedded_https_test_server().GetURL(kHostname, kStartUrl);
  }

  GURL GetOutOfScopeUrl() {
    return embedded_https_test_server().GetURL(kHostname, kOutOfScopeUrl);
  }

  GURL GetInstallUrl() {
    return embedded_https_test_server().GetURL(kHostname, kInstallUrl);
  }

  GURL GetScope() {
    return embedded_https_test_server().GetURL(kHostname, kScope);
  }

  webapps::ManifestId GetManifestId() {
    return GenerateManifestIdFromStartUrlOnly(GetStartUrl());
  }

  GURL GetManifestUrl() {
    return embedded_https_test_server().GetURL(kHostname, kManifestUrl);
  }

  webapps::AppId GetAppId() {
    return GenerateAppIdFromManifestId(GetManifestId());
  }

  SkBitmap LoadPngImageFromDisk(base::FilePath relative_test_file) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &path);
    base::FilePath image_file = path.Append(relative_test_file);
    CHECK(base::PathExists(image_file)) << image_file.value();
    std::optional<std::vector<uint8_t>> file_contents =
        base::ReadFileToBytes(image_file);
    CHECK(file_contents.has_value());
    SkBitmap png_bytes = gfx::PNGCodec::Decode(file_contents.value());
    CHECK(!png_bytes.empty());
    return png_bytes;
  }

  ExternalInstallOptions GetInstallOptionsWithFactory() {
    ExternalInstallOptions options(
        /*install_url=*/GetInstallUrl(),
        /*user_display_mode=*/
        mojom::UserDisplayMode::kBrowser,
        /*install_source=*/ExternalInstallSource::kExternalDefault);

    options.user_type_allowlist = {"unmanaged", "managed", "child"};
    options.expected_app_id = GetAppId();

    IconBitmaps icons;
    icons.any = {{48, LoadPngImageFromDisk(base::FilePath(kIcon48))},
                 {192, LoadPngImageFromDisk(base::FilePath(kIcon192))}};

    options.app_info_factory = base::BindRepeating(
        [](webapps::ManifestId manifest_id, GURL start_url, GURL scope,
           GURL install_url, GURL manifest_url, IconBitmaps icons) {
          auto info =
              std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
          info->title = kWrongName;
          info->scope = scope;
          info->display_mode = DisplayMode::kStandalone;
          info->install_url = install_url;
          info->icon_bitmaps = std::move(icons);
          info->manifest_url = manifest_url;
          return info;
        },
        GetManifestId(), GetStartUrl(), GetScope(), GetInstallUrl(),
        GetManifestUrl(), std::move(icons));
    options.only_use_app_info_factory = true;

    return options;
  }

  void SetUp() override {
    embedded_https_test_server().AddDefaultHandlers(GetChromeTestDataDir());
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &PreinstalledWebAppManagerSimpleBrowserTest::SetRedirectHandler,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_https_test_server().Start());
    preinstalled_app_override_ =
        std::make_unique<ScopedTestingPreinstalledAppData>();
    preinstalled_app_override_->apps = {GetInstallOptionsWithFactory()};
    WebAppBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    test::WaitUntilWebAppProviderAndSubsystemsReady(&provider());
  }

  void StartRedirecting() { is_redirection_on_ = true; }

  // Handler to redirect from the GetStartUrl() to GetOutOfScopeUrl()
  std::unique_ptr<net::test_server::HttpResponse> SetRedirectHandler(
      const net::test_server::HttpRequest& request) {
    if (!is_redirection_on_) {
      return nullptr;
    }
    if (request.relative_url != GetStartUrl().PathForRequest()) {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    std::string destination = request.GetURL().spec() + "/redirected";
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->set_content_type("text/html");
    response->AddCustomHeader("Location", GetOutOfScopeUrl().spec());
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_content(
        base::StringPrintf("<!doctype html><p>Redirecting to %s",
                           GetOutOfScopeUrl().spec().c_str()));
    return response;
  }

 protected:
  base::RepeatingClosure run_delayed_startup_tasks_;

 private:
  bool is_redirection_on_ = false;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kWebAppPeriodicPreinstallUpdate};

  std::unique_ptr<ScopedTestingPreinstalledAppData> preinstalled_app_override_;

  std::unique_ptr<FakeWebAppProviderCreator> fake_provider_creator_;

  base::WeakPtrFactory<PreinstalledWebAppManagerSimpleBrowserTest>
      weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerSimpleBrowserTest,
                       PreinstallWorks) {
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      GetAppId(), WebAppFilter::InstalledInChrome()));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerSimpleBrowserTest,
                       DelayedUpdateWorks) {
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      GetAppId(), WebAppFilter::InstalledInChrome()));

  run_delayed_startup_tasks_.Run();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(GetAppId()),
            "Simple web app");
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerSimpleBrowserTest,
                       NoUpdateOnRedirectedBrowserDisplayModeLaunch) {
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      GetAppId(), WebAppFilter::InstalledInChrome()));

  StartRedirecting();

  base::test::TestFuture<base::WeakPtr<BrowserWindowInterface>,
                         base::WeakPtr<content::WebContents>,
                         apps::LaunchContainer>
      launch;
  provider().scheduler().LaunchApp(GetAppId(), /*url=*/std::nullopt,
                                   launch.GetCallback());
  ASSERT_TRUE(launch.Wait());
  base::WeakPtr<content::WebContents> web_contents =
      launch.Get<base::WeakPtr<content::WebContents>>();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(test::WebAppPageWaiter(web_contents.get())
                  .ExpectUrl(GetOutOfScopeUrl())
                  .ManifestOrLoadedNoManifest()
                  .WaitAndFlushCommands());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(GetAppId()),
            "Wrong App Name");
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppManagerSimpleBrowserTest,
                       UpdateOnFirstLaunchRedirect) {
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      GetAppId(), WebAppFilter::InstalledInChrome()));

  provider().scheduler().SetUserDisplayMode(
      GetAppId(), mojom::UserDisplayMode::kStandalone, base::DoNothing());

  StartRedirecting();

  base::HistogramTester histogram_tester;
  base::test::TestFuture<base::WeakPtr<BrowserWindowInterface>,
                         base::WeakPtr<content::WebContents>,
                         apps::LaunchContainer>
      launch;
  provider().scheduler().LaunchApp(GetAppId(), /*url=*/std::nullopt,
                                   launch.GetCallback());
  ASSERT_TRUE(launch.Wait());
  base::WeakPtr<content::WebContents> web_contents =
      launch.Get<base::WeakPtr<content::WebContents>>();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(test::WebAppPageWaiter(web_contents.get())
                  .ExpectUrl(GetOutOfScopeUrl())
                  .ManifestOrLoadedNoManifest()
                  .WaitAndFlushCommands());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  histogram_tester.ExpectUniqueSample("WebApp.FetchManifestAndUpdate.Result",
                                      FetchManifestAndUpdateResult::kSuccess,
                                      1);

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(GetAppId()),
            "Simple web app");
}

}  // namespace web_app
