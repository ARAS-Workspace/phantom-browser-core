// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_registration_task.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/external_app_registration_waiter.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/origin.h"

namespace web_app {

class ExternallyManagedAppManagerBrowserTest : public WebAppBrowserTestBase {
 public:
  std::unique_ptr<net::test_server::HttpResponse> SimulateRedirectHandler(
      const net::test_server::HttpRequest& request) {
    if (!simulate_redirect_) {
      // Fall back to default handlers.
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (request.GetURL().spec().find("redirected") != std::string::npos) {
      response->set_code(net::HTTP_MOVED_PERMANENTLY);
      response->set_content("Redirect successful");
      return response;
    }

    std::string destination = request.GetURL().spec() + "/redirected";
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->set_content_type("text/html");
    response->AddCustomHeader("Location", destination);
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_content(base::StringPrintf(
        "<!doctype html><p>Redirecting to %s", destination.c_str()));
    return response;
  }

 protected:
  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    // Allow different origins to be handled by the embedded_test_server.
    host_resolver()->AddRule("*", "127.0.0.1");
    test::WaitUntilWebAppProviderAndSubsystemsReady(provider());
  }

  Profile* profile() { return browser()->GetProfile(); }

  WebAppRegistrar& registrar() { return provider()->registrar_unsafe(); }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  ExternallyManagedAppManager& externally_managed_app_manager() {
    return provider()->externally_managed_app_manager();
  }

  void InstallApp(ExternalInstallOptions install_options) {
    auto result = ExternallyManagedAppManagerInstall(
        profile(), std::move(install_options));
    result_code_ = result.code;
  }

  void CheckServiceWorkerStatus(const GURL& url,
                                content::ServiceWorkerCapability status) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile()));
    content::StoragePartition* storage_partition =
        web_contents->GetBrowserContext()->GetStoragePartition(
            web_contents->GetSiteInstance());
    test::CheckServiceWorkerStatus(url, storage_partition, status);
  }

  std::optional<webapps::InstallResultCode> result_code_;
  bool simulate_redirect_ = false;
};

// Basic integration test to make sure the whole flow works. Each step in the
// flow is unit tested separately.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       InstallSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  InstallApp(CreateInstallOptions(url));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  EXPECT_TRUE(app_id.has_value());
  EXPECT_EQ("Manifest test app", registrar().GetAppShortName(app_id.value()));
}

// If install URL redirects, install should still succeed.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       InstallSucceedsWithRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL start_url =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  GURL install_url =
      embedded_test_server()->GetURL("/server-redirect?" + start_url.spec());
  // TODO(crbug.com/381408483): Review usage of ExternalInstallOptions in tests.
  ExternalInstallOptions install_options(
      install_url, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  InstallApp(install_options);
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(install_url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_EQ("Manifest test app", registrar().GetAppShortName(app_id.value()));
  // Same AppID should be in the registrar using start_url from the manifest.
  EXPECT_TRUE(registrar().AppMatches(
      app_id.value(), WebAppFilter::InstalledInOperatingSystemForTesting()));
  std::optional<webapps::AppId> opt_app_id =
      registrar().FindBestAppWithUrlInScope(
          start_url,
          web_app::WebAppFilter::InstalledInOperatingSystemForTesting());
  EXPECT_TRUE(opt_app_id.has_value());
  EXPECT_EQ(*opt_app_id, app_id);
}

// If install URL redirects, install should still succeed.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       InstallSucceedsWithRedirectNoManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL final_url =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  GURL install_url =
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec());
  InstallApp(CreateInstallOptions(install_url));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(install_url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_EQ("Web app banner test page",
            registrar().GetAppShortName(app_id.value()));
  std::optional<webapps::AppId> opt_app_id =
      registrar().FindBestAppWithUrlInScope(
          final_url,
          web_app::WebAppFilter::InstalledInOperatingSystemForTesting());
  ASSERT_TRUE(opt_app_id.has_value());
  EXPECT_EQ(*opt_app_id, app_id);
  EXPECT_EQ(registrar().GetAppStartUrl(*opt_app_id), final_url);
}

// Installing a placeholder app with shortcuts should succeed.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       PlaceholderInstallSucceedsWithShortcuts) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL final_url = embedded_test_server()->GetURL(
      "other.origin.com", "/banners/manifest_test_page.html");
  // Add a redirect to a different origin, so a placeholder is installed.
  GURL url(
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec()));

  ExternalInstallOptions options =
      CreateInstallOptions(url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(
      registrar().IsPlaceholderApp(app_id.value(), WebAppManagement::kPolicy));
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       UpdatePlaceholderSucceedsSameAppId) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &ExternallyManagedAppManagerBrowserTest::SimulateRedirectHandler,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());

  simulate_redirect_ = true;
  GURL url = embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  ExternalInstallOptions options =
      CreateInstallOptions(url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(
      registrar().IsPlaceholderApp(app_id.value(), WebAppManagement::kPolicy));

  simulate_redirect_ = false;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> final_app_id =
      registrar().LookupExternalAppId(url);
  ASSERT_TRUE(final_app_id.has_value());
  EXPECT_FALSE(registrar().IsPlaceholderApp(final_app_id.value(),
                                            WebAppManagement::kPolicy));
  EXPECT_EQ(0, registrar().CountUserInstalledApps());
  EXPECT_EQ(1u, registrar()
                    .GetExternallyInstalledApps(
                        ExternalInstallSource::kExternalPolicy)
                    .size());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       InstallPlaceholderAppWindowOpen) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &ExternallyManagedAppManagerBrowserTest::SimulateRedirectHandler,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());

  simulate_redirect_ = true;
  GURL url = embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  ExternalInstallOptions options =
      CreateInstallOptions(url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(
      registrar().IsPlaceholderApp(app_id.value(), WebAppManagement::kPolicy));

  // Open an app window so that the placeholder resolution is delayed.
  Browser* app_browser = LaunchWebAppBrowser(app_id.value());
  EXPECT_NE(nullptr, app_browser);
  options.placeholder_resolution_behavior =
      PlaceholderResolutionBehavior::kWaitForAppWindowsClosed;

  base::test::TestFuture<const GURL&,
                         ExternallyManagedAppManager::InstallResult>
      install_future;
  simulate_redirect_ = false;
  provider()->externally_managed_app_manager().Install(
      std::move(options), install_future.GetCallback());

  // The callback will not be run since there is an existing app window that is
  // open.
  EXPECT_FALSE(install_future.IsReady());
  EXPECT_TRUE(
      registrar().IsPlaceholderApp(app_id.value(), WebAppManagement::kPolicy));

  // Once the app window is closed, placeholder resolution should happen.
  chrome::CloseWindow(app_browser);
  EXPECT_TRUE(install_future.Wait());

  EXPECT_EQ(
      webapps::InstallResultCode::kSuccessNewInstall,
      install_future.Get<ExternallyManagedAppManager::InstallResult>().code);
  std::optional<webapps::AppId> final_app_id =
      registrar().LookupExternalAppId(url);
  ASSERT_TRUE(final_app_id.has_value());
  EXPECT_FALSE(registrar().IsPlaceholderApp(final_app_id.value(),
                                            WebAppManagement::kPolicy));
  EXPECT_EQ(0, registrar().CountUserInstalledApps());
  EXPECT_EQ(1u, registrar()
                    .GetExternallyInstalledApps(
                        ExternalInstallSource::kExternalPolicy)
                    .size());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       PlaceholderResolutionNoAppWindow) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &ExternallyManagedAppManagerBrowserTest::SimulateRedirectHandler,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());

  simulate_redirect_ = true;
  GURL url = embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  ExternalInstallOptions options =
      CreateInstallOptions(url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(
      registrar().IsPlaceholderApp(app_id.value(), WebAppManagement::kPolicy));

  // Since no app windows are open, placeholders are updated instantly.
  options.placeholder_resolution_behavior =
      PlaceholderResolutionBehavior::kWaitForAppWindowsClosed;
  simulate_redirect_ = false;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> final_app_id =
      registrar().LookupExternalAppId(url);
  ASSERT_TRUE(final_app_id.has_value());
  EXPECT_FALSE(registrar().IsPlaceholderApp(final_app_id.value(),
                                            WebAppManagement::kPolicy));
  EXPECT_EQ(0, registrar().CountUserInstalledApps());
  EXPECT_EQ(1u, registrar()
                    .GetExternallyInstalledApps(
                        ExternalInstallSource::kExternalPolicy)
                    .size());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       UpdatePlaceholderSucceedsDifferentAppIdFomStartUrl) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &ExternallyManagedAppManagerBrowserTest::SimulateRedirectHandler,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());

  simulate_redirect_ = true;
  GURL install_url = embedded_test_server()->GetURL(
      "/banners/manifest_with_start_url_test_page.html");
  ExternalInstallOptions options =
      CreateInstallOptions(install_url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  const webapps::AppId placeholder_app_id =
      GenerateAppId(std::nullopt, install_url);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(install_url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_EQ(placeholder_app_id, *app_id);
  EXPECT_TRUE(registrar().IsPlaceholderApp(*app_id, WebAppManagement::kPolicy));

  simulate_redirect_ = false;
  InstallApp(options);

  GURL start_url = embedded_test_server()->GetURL(
      "/banners/different_manifest_test_page.html");
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());

  const webapps::AppId new_app_id = GenerateAppId(std::nullopt, start_url);

  EXPECT_NE(new_app_id, placeholder_app_id);
  EXPECT_FALSE(registrar().GetInstallState(placeholder_app_id).has_value());
  EXPECT_TRUE(registrar().AppMatches(
      new_app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  EXPECT_FALSE(
      registrar().IsPlaceholderApp(new_app_id, WebAppManagement::kPolicy));
  EXPECT_EQ(0, registrar().CountUserInstalledApps());
  EXPECT_EQ(1u, registrar()
                    .GetExternallyInstalledApps(
                        ExternalInstallSource::kExternalPolicy)
                    .size());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       UpdatePlaceholderSucceedsDifferentAppIdFomManifestId) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &ExternallyManagedAppManagerBrowserTest::SimulateRedirectHandler,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());

  simulate_redirect_ = true;
  GURL install_url = embedded_test_server()->GetURL(
      "/banners/manifest_with_id_test_page.html");
  ExternalInstallOptions options =
      CreateInstallOptions(install_url, ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  const webapps::AppId placeholder_app_id =
      GenerateAppId(std::nullopt, install_url);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id =
      registrar().LookupExternalAppId(install_url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_EQ(placeholder_app_id, *app_id);
  EXPECT_TRUE(registrar().IsPlaceholderApp(*app_id, WebAppManagement::kPolicy));

  simulate_redirect_ = false;
  InstallApp(options);

  GURL start_url = embedded_test_server()->GetURL("/banners/start");
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());

  const webapps::AppId new_app_id = GenerateAppId("some_id", start_url);

  EXPECT_NE(new_app_id, placeholder_app_id);
  EXPECT_FALSE(registrar().GetInstallState(placeholder_app_id).has_value());
  EXPECT_TRUE(registrar().AppMatches(
      new_app_id, WebAppFilter::InstalledInOperatingSystemForTesting()));
  EXPECT_FALSE(
      registrar().IsPlaceholderApp(new_app_id, WebAppManagement::kPolicy));
  EXPECT_EQ(0, registrar().CountUserInstalledApps());
  EXPECT_EQ(1u, registrar()
                    .GetExternallyInstalledApps(
                        ExternalInstallSource::kExternalPolicy)
                    .size());
}

// Tests that the browser doesn't crash if it gets shutdown with a pending
// installation.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       ShutdownWithPendingInstallation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ExternalInstallOptions install_options = CreateInstallOptions(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  // Start an installation but don't wait for it to finish.
  provider()->externally_managed_app_manager().Install(
      std::move(install_options), base::DoNothing());

  // The browser should shutdown cleanly even if there is a pending
  // installation.
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest, ForceReinstall) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::optional<webapps::AppId> app_id;
  {
    GURL url(embedded_test_server()->GetURL(
        "/banners/"
        "manifest_test_page.html?manifest=manifest_short_name_only.json"));
    ExternalInstallOptions install_options = CreateInstallOptions(url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));

    app_id = registrar().FindBestAppWithUrlInScope(
        url, web_app::WebAppFilter::InstalledInOperatingSystemForTesting());
    EXPECT_TRUE(app_id.has_value());
    EXPECT_EQ("Manifest", registrar().GetAppShortName(app_id.value()));
  }
  {
    GURL url(
        embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
    ExternalInstallOptions install_options = CreateInstallOptions(url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));

    std::optional<webapps::AppId> new_app_id =
        registrar().FindBestAppWithUrlInScope(
            url, web_app::WebAppFilter::InstalledInOperatingSystemForTesting());
    EXPECT_TRUE(new_app_id.has_value());
    EXPECT_EQ(new_app_id, app_id);
    EXPECT_EQ("Manifest test app",
              registrar().GetAppShortName(new_app_id.value()));
  }
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       PolicyAppOverridesUserInstalledApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  std::optional<webapps::AppId> app_id;
  {
    // Install user app
    GURL url(
        embedded_test_server()->GetURL("/banners/"
                                       "manifest_test_page.html"));
    auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(url);
    install_info->title = u"Test user app";
    app_id = test::InstallWebApp(profile(), std::move(install_info));
    ASSERT_TRUE(app_id.has_value());
    ASSERT_TRUE(registrar().AppMatches(app_id.value(),
                                       WebAppFilter::InstalledByUser()));
    ASSERT_TRUE(registrar()
                    .GetAppById(app_id.value())
                    ->management_to_external_config_map()
                    .empty());
    ASSERT_EQ("Test user app", registrar().GetAppShortName(app_id.value()));
  }
  {
    // Install policy app
    GURL url(
        embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
    std::optional<webapps::AppId> policy_app_id =
        ForceInstallWebApp(profile(), url);
    ASSERT_EQ(policy_app_id, app_id);
    ASSERT_EQ("Manifest test app",
              registrar().GetAppShortName(policy_app_id.value()));
  }
}

// Test that adding a manifest that points to a chrome:// URL does not actually
// install a web app that points to a chrome:// URL.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       InstallChromeURLFails) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=manifest_chrome_url.json"));
  InstallApp(CreateInstallOptions(url));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  ASSERT_TRUE(app_id.has_value());

  // The installer falls back to installing a web app of the original URL.
  EXPECT_EQ(url, registrar().GetAppStartUrl(app_id.value()));
  EXPECT_NE(app_id,
            registrar().FindBestAppWithUrlInScope(
                GURL("chrome://settings"),
                web_app::WebAppFilter::InstalledInOperatingSystemForTesting()));
}

// Test that adding a web app without a manifest while using the
// |require_manifest| flag fails.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       RequireManifestFailsIfNoManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"));
  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.require_manifest = true;
  InstallApp(std::move(install_options));

  EXPECT_EQ(webapps::InstallResultCode::kNotValidManifestForWebApp,
            result_code_.value());
  std::optional<webapps::AppId> id = registrar().LookupExternalAppId(url);
  ASSERT_FALSE(id.has_value());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       RegistrationSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Delay service worker registration to second load to simulate it not loading
  // during the initial install pass.
  GURL install_url(embedded_test_server()->GetURL(
      "/web_apps/service_worker_on_second_load.html"));

  ExternalInstallOptions install_options = CreateInstallOptions(install_url);
  InstallApp(std::move(install_options));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  ExternalAppRegistrationWaiter(&externally_managed_app_manager())
      .AwaitNextRegistration(install_url, RegistrationResultCode::kSuccess);
  CheckServiceWorkerStatus(
      install_url,
      content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       RegistrationAlternateUrlSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL install_url(
      embedded_test_server()->GetURL("/web_apps/no_service_worker.html"));
  GURL registration_url =
      embedded_test_server()->GetURL("/web_apps/basic.html");

  ExternalInstallOptions install_options = CreateInstallOptions(install_url);
  install_options.service_worker_registration_url = registration_url;
  InstallApp(std::move(install_options));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  ExternalAppRegistrationWaiter(&externally_managed_app_manager())
      .AwaitNextRegistration(registration_url,
                             RegistrationResultCode::kSuccess);
  CheckServiceWorkerStatus(
      install_url,
      content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       RegistrationSkipped) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Delay service worker registration to second load to simulate it not loading
  // during the initial install pass.
  GURL install_url(embedded_test_server()->GetURL(
      "/web_apps/service_worker_on_second_load.html"));

  ExternalInstallOptions install_options = CreateInstallOptions(install_url);
  install_options.load_and_await_service_worker_registration = false;
  ExternalAppRegistrationWaiter waiter(&externally_managed_app_manager());
  InstallApp(std::move(install_options));
  waiter.AwaitRegistrationsComplete();

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  CheckServiceWorkerStatus(install_url,
                           content::ServiceWorkerCapability::NO_SERVICE_WORKER);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       ServiceWorkerRegistrationSkippedForChromeScheme) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL install_url("chrome://web-app-internals/");

  ExternalInstallOptions install_options = CreateInstallOptions(install_url);
  install_options.load_and_await_service_worker_registration = false;
  ExternalAppRegistrationWaiter waiter(&externally_managed_app_manager());
  InstallApp(std::move(install_options));
  waiter.AwaitRegistrationsComplete();

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  CheckServiceWorkerStatus(install_url,
                           content::ServiceWorkerCapability::NO_SERVICE_WORKER);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       AlreadyRegistered) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Ensure service worker registered for http://embedded_test_server/web_apps/.
  // We don't need to be installing a web app here but it's convenient just to
  // await the service worker registration.
  {
    GURL install_url(embedded_test_server()->GetURL("/web_apps/basic.html"));
    ExternalInstallOptions install_options = CreateInstallOptions(install_url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
              result_code_.value());
    ExternalAppRegistrationWaiter(&externally_managed_app_manager())
        .AwaitNextNonFailedRegistration(install_url);
    CheckServiceWorkerStatus(
        embedded_test_server()->GetURL("/web_apps/basic.html"),
        content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
  }

  // With the service worker registered we install a page that doesn't register
  // a service worker to check that the existing service worker is seen by our
  // service worker registration step.
  {
    GURL install_url(
        embedded_test_server()->GetURL("/web_apps/no_service_worker.html"));
    ExternalInstallOptions install_options = CreateInstallOptions(install_url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));
    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
              result_code_.value());
    ExternalAppRegistrationWaiter(&externally_managed_app_manager())
        .AwaitNextRegistration(install_url,
                               RegistrationResultCode::kAlreadyRegistered);
  }
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       CannotFetchManifest) {
  // With a flaky network connection, clients may request an app whose manifest
  // cannot currently be retrieved. The app display mode is then assumed to be
  // 'browser'.
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=does_not_exist.json"));

  std::vector<ExternalInstallOptions> desired_apps_install_options;
  {
    ExternalInstallOptions install_options(
        app_url, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    install_options.add_to_applications_menu = false;
    install_options.add_to_desktop = false;
    install_options.add_to_quick_launch_bar = false;
    install_options.require_manifest = false;
    desired_apps_install_options.push_back(std::move(install_options));
  }

  base::RunLoop run_loop;
  externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(desired_apps_install_options),
      ExternalInstallSource::kExternalPolicy,
      base::BindLambdaForTesting(
          [&run_loop, &app_url](
              std::map<GURL, ExternallyManagedAppManager::InstallResult>
                  install_results,
              std::map<GURL, webapps::UninstallResultCode> uninstall_results) {
            EXPECT_TRUE(uninstall_results.empty());
            EXPECT_EQ(install_results.size(), 1U);
            EXPECT_EQ(install_results[app_url].code,
                      webapps::InstallResultCode::kSuccessNewInstall);
            run_loop.Quit();
          }));
  run_loop.Run();

  std::optional<webapps::AppId> app_id = registrar().FindBestAppWithUrlInScope(
      app_url, web_app::WebAppFilter::InstalledInOperatingSystemForTesting());
  DCHECK(app_id.has_value());
  EXPECT_EQ(registrar().GetAppDisplayMode(*app_id), DisplayMode::kBrowser);
  EXPECT_EQ(registrar().GetAppUserDisplayMode(*app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar().GetAppEffectiveDisplayMode(*app_id),
            DisplayMode::kMinimalUi);
  EXPECT_FALSE(registrar().GetAppThemeColor(*app_id).has_value());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       RegistrationTimeout) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html"));
  CheckServiceWorkerStatus(url,
                           content::ServiceWorkerCapability::NO_SERVICE_WORKER);

  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.service_worker_registration_timeout = base::Seconds(0);
  InstallApp(std::move(install_options));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  ExternalAppRegistrationWaiter(&externally_managed_app_manager())
      .AwaitNextRegistration(url, RegistrationResultCode::kTimeout);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerBrowserTest,
                       ReinstallPolicyAppWithLocallyInstalledApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  // Install user app
  auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(url);
  install_info->title = u"Test user app";
  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(install_info));
  ASSERT_TRUE(registrar().AppMatches(app_id, WebAppFilter::InstalledByUser()));
  ASSERT_TRUE(registrar()
                  .GetAppById(app_id)
                  ->management_to_external_config_map()
                  .empty());

  // Install policy app
  std::optional<webapps::AppId> policy_app_id =
      ForceInstallWebApp(profile(), url);
  ASSERT_EQ(policy_app_id, app_id);

  // Uninstall policy app
  std::vector<ExternalInstallOptions> desired_apps_install_options;
  base::RunLoop run_loop;
  externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(desired_apps_install_options),
      ExternalInstallSource::kExternalPolicy,
      base::BindLambdaForTesting(
          [&run_loop, &url](
              std::map<GURL, ExternallyManagedAppManager::InstallResult>
                  install_results,
              std::map<GURL, webapps::UninstallResultCode> uninstall_results) {
            EXPECT_TRUE(install_results.empty());
            EXPECT_EQ(uninstall_results.size(), 1U);
            EXPECT_EQ(uninstall_results[url],
                      webapps::UninstallResultCode::kInstallSourceRemoved);
            run_loop.Quit();
          }));
  run_loop.Run();
  ASSERT_FALSE(registrar().GetAppById(app_id)->IsPolicyInstalledApp());

  // Reinstall policy app
  ForceInstallWebApp(profile(), url);
  ASSERT_TRUE(registrar().GetAppById(app_id)->IsPolicyInstalledApp());
}

class ExternallyManagedAppManagerBrowserTestShortcut
    : public ExternallyManagedAppManagerBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ExternallyManagedAppManagerBrowserTestShortcut() = default;
};

// Tests behavior when ExternalInstallOptions.install_as_shortcut is enabled
IN_PROC_BROWSER_TEST_P(ExternallyManagedAppManagerBrowserTestShortcut,
                       InstallAsShortcut) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL install_url(
      embedded_test_server()->GetURL("/web_apps/different_start_url.html"));
  GURL manifest_start_url(
      embedded_test_server()->GetURL("/web_apps/basic.html"));

  ExternalInstallOptions options =
      CreateInstallOptions(install_url, ExternalInstallSource::kExternalPolicy);
  options.install_as_diy = GetParam();

  InstallApp(options);
  ASSERT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());

  // The main difference between a normal web app installation and a shortcut
  // creation is that in the latter the start_url field of the page's manifest
  // is ignored. Thus the installation URL is always used even when the
  // manifest tells otherwise, as in the test page used here.

  const bool startUrlIsInstallUrl =
      registrar().GetAppByStartUrl(install_url) != nullptr;
  const bool startUrlFromManifest =
      registrar().GetAppByStartUrl(manifest_start_url) != nullptr;
  EXPECT_NE(startUrlIsInstallUrl, startUrlFromManifest);

  EXPECT_EQ(options.install_as_diy, startUrlIsInstallUrl);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExternallyManagedAppManagerBrowserTestShortcut,
                         ::testing::Bool());

}  // namespace web_app
