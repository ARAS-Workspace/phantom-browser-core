// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation_traits.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "extensions/browser/extension_registrar.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

const base::Time kLastLaunchTime = base::Time::Now();
const base::Time kInstallTime = base::Time::Now();
const char kUrl[] = "https://example.com/";

scoped_refptr<extensions::Extension> MakeExtensionApp(
    const std::string& name,
    const std::string& version,
    const std::string& url,
    const std::string& id) {
  std::u16string err;
  base::DictValue value;
  value.Set("name", name);
  value.Set("version", version);
  base::ListValue scripts;
  scripts.Append("script.js");
  value.SetByDottedPath("app.background.scripts", std::move(scripts));
  scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kInternal, value,
      extensions::Extension::WAS_INSTALLED_BY_DEFAULT, id, &err);
  EXPECT_EQ(err, u"");
  return app;
}

apps::IntentFilters CreateIntentFilters() {
  const GURL url(kUrl);
  apps::IntentFilters filters;
  apps::IntentFilterPtr filter = std::make_unique<apps::IntentFilter>();

  apps::ConditionValues values1;
  values1.push_back(std::make_unique<apps::ConditionValue>(
      apps_util::kIntentActionView, apps::PatternMatchType::kLiteral));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kAction, std::move(values1)));

  apps::ConditionValues values2;
  values2.push_back(std::make_unique<apps::ConditionValue>(
      url.GetScheme(), apps::PatternMatchType::kLiteral));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kScheme, std::move(values2)));

  apps::ConditionValues values3;
  values3.push_back(std::make_unique<apps::ConditionValue>(
      apps_util::AuthorityView::Encode(url), apps::PatternMatchType::kLiteral));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kAuthority, std::move(values3)));

  apps::ConditionValues values4;
  values4.push_back(std::make_unique<apps::ConditionValue>(
      url.GetPath(), apps::PatternMatchType::kPrefix));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kPath, std::move(values4)));

  filters.push_back(std::move(filter));

  return filters;
}

MATCHER(Ready, "App has readiness=\"kReady\"") {
  return arg.readiness == apps::Readiness::kReady;
}

MATCHER_P(ShownInShelf, shown, "App shown on the shelf") {
  return arg.show_in_shelf.has_value() && arg.show_in_shelf == shown;
}

MATCHER_P(ShownInLauncher, shown, "App shown in the launcher") {
  return arg.show_in_launcher.has_value() && arg.show_in_launcher == shown;
}

// AppRegistryCacheObserver is used to test the OnAppTypeInitialized and
// OnAppUpdate interfaces for AppRegistryCache::Observer.
class AppRegistryCacheObserver : public apps::AppRegistryCache::Observer {
 public:
  explicit AppRegistryCacheObserver(apps::AppRegistryCache* cache) {
    app_registry_cache_observer_.Observe(cache);
  }

  ~AppRegistryCacheObserver() override = default;

  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const apps::AppUpdate& update) override {
    updated_ids_.push_back(update.AppId());
  }

  void OnAppTypeInitialized(apps::AppType app_type) override {
    app_types_.push_back(app_type);
  }

  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    app_registry_cache_observer_.Reset();
  }

  std::vector<std::string> updated_ids() const { return updated_ids_; }
  std::vector<apps::AppType> app_types() const { return app_types_; }

 private:
  std::vector<std::string> updated_ids_;
  std::vector<apps::AppType> app_types_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

}  // namespace

namespace apps {

class PublisherTest : public extensions::ExtensionServiceTestBase {
 public:
  PublisherTest() = default;
  PublisherTest(const PublisherTest&) = delete;
  PublisherTest& operator=(const PublisherTest&) = delete;

  ~PublisherTest() override = default;

  // ExtensionServiceTestBase:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams());
    service()->Init();
    ConfigureWebAppProvider();
  }

  void TearDown() override {
    extensions::ExtensionServiceTestBase::TearDown();
  }

  void ConfigureWebAppProvider() {
    auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();

    auto externally_managed_app_manager =
        std::make_unique<web_app::ExternallyManagedAppManager>(profile());
    externally_managed_app_manager->SetUrlLoaderForTesting(
        std::move(url_loader));

    auto* const provider = web_app::FakeWebAppProvider::Get(profile());
    provider->SetExternallyManagedAppManager(
        std::move(externally_managed_app_manager));
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    base::RunLoop().RunUntilIdle();
  }

  std::string CreateWebApp(const std::string& app_name) {
    const GURL kAppUrl(kUrl);

    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(kAppUrl);
    web_app_info->title = base::UTF8ToUTF16(app_name);
    web_app_info->scope = kAppUrl;
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;

    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  void VerifyOptionalBool(std::optional<bool> source,
                          std::optional<bool> target) {
    if (source.has_value()) {
      EXPECT_EQ(source, target);
    }
  }

  const AppPtr& GetApp(const std::string& app_id) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
    return cache.states_[app_id];
  }

  void VerifyNoApp(const std::string& app_id) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();

    ASSERT_EQ(cache.states_.end(), cache.states_.find(app_id));
  }

  void VerifyApp(AppType app_type,
                 const std::string& app_id,
                 const std::string& name,
                 apps::Readiness readiness,
                 InstallReason install_reason,
                 InstallSource install_source,
                 const std::vector<std::string>& additional_search_terms,
                 base::Time last_launch_time,
                 base::Time install_time,
                 const apps::Permissions& permissions,
                 std::optional<bool> is_platform_app = std::nullopt,
                 std::optional<bool> recommendable = std::nullopt,
                 std::optional<bool> searchable = std::nullopt,
                 std::optional<bool> show_in_launcher = std::nullopt,
                 std::optional<bool> show_in_shelf = std::nullopt,
                 std::optional<bool> show_in_search = std::nullopt,
                 std::optional<bool> show_in_management = std::nullopt,
                 std::optional<bool> handles_intents = std::nullopt,
                 std::optional<bool> allow_uninstall = std::nullopt,
                 std::optional<bool> allow_close = std::nullopt,
                 std::optional<bool> has_badge = std::nullopt,
                 std::optional<bool> paused = std::nullopt,
                 std::optional<bool> allow_window_mode_selection = std::nullopt,
                 WindowMode window_mode = WindowMode::kUnknown) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();

    ASSERT_NE(cache.states_.end(), cache.states_.find(app_id));
    EXPECT_EQ(app_type, cache.states_[app_id]->app_type);
    ASSERT_TRUE(cache.states_[app_id]->name.has_value());
    EXPECT_EQ(name, cache.states_[app_id]->name.value());
    EXPECT_EQ(readiness, cache.states_[app_id]->readiness);
    ASSERT_TRUE(cache.states_[app_id]->icon_key.has_value());
    EXPECT_EQ(install_reason, cache.states_[app_id]->install_reason);
    EXPECT_EQ(install_source, cache.states_[app_id]->install_source);
    EXPECT_EQ(additional_search_terms,
              cache.states_[app_id]->additional_search_terms);
    if (!last_launch_time.is_null()) {
      EXPECT_EQ(last_launch_time, cache.states_[app_id]->last_launch_time);
    }
    if (!install_time.is_null()) {
      EXPECT_EQ(install_time, cache.states_[app_id]->install_time);
    }
    if (!permissions.empty()) {
      EXPECT_TRUE(IsEqual(permissions, cache.states_[app_id]->permissions));
    }

    VerifyOptionalBool(is_platform_app, cache.states_[app_id]->is_platform_app);
    VerifyOptionalBool(recommendable, cache.states_[app_id]->recommendable);
    VerifyOptionalBool(searchable, cache.states_[app_id]->searchable);
    VerifyOptionalBool(show_in_launcher,
                       cache.states_[app_id]->show_in_launcher);
    VerifyOptionalBool(show_in_shelf, cache.states_[app_id]->show_in_shelf);
    VerifyOptionalBool(show_in_search, cache.states_[app_id]->show_in_search);
    VerifyOptionalBool(show_in_management,
                       cache.states_[app_id]->show_in_management);
    VerifyOptionalBool(handles_intents, cache.states_[app_id]->handles_intents);
    VerifyOptionalBool(allow_uninstall, cache.states_[app_id]->allow_uninstall);
    VerifyOptionalBool(allow_close, cache.states_[app_id]->allow_close);
    VerifyOptionalBool(has_badge, cache.states_[app_id]->has_badge);
    VerifyOptionalBool(paused, cache.states_[app_id]->paused);
    VerifyOptionalBool(allow_window_mode_selection,
                       cache.states_[app_id]->allow_window_mode_selection);
    if (window_mode != WindowMode::kUnknown) {
      EXPECT_EQ(window_mode, cache.states_[app_id]->window_mode);
    }
  }

  void VerifyAppIsRemoved(const std::string& app_id) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
    ASSERT_NE(cache.states_.end(), cache.states_.find(app_id));
    EXPECT_EQ(apps::Readiness::kUninstalledByUser,
              cache.states_[app_id]->readiness);
  }

  void VerifyIntentFilters(const std::string& app_id) {
    apps::IntentFilters source = CreateIntentFilters();

    apps::IntentFilters target;
    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->AppRegistryCache()
        .ForOneApp(app_id, [&target](const apps::AppUpdate& update) {
          target = update.IntentFilters();
        });

    EXPECT_EQ(source.size(), target.size());
    for (int i = 0; i < static_cast<int>(source.size()); i++) {
      EXPECT_EQ(*source[i], *target[i]);
    }
  }

  void VerifyAppTypeIsInitialized(AppType app_type) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
    ASSERT_TRUE(cache.IsAppTypeInitialized(app_type));
    ASSERT_TRUE(cache.InitializedAppTypes().contains(app_type));
  }

  void VerifyCapabilityAccess(const std::string& app_id,
                              std::optional<bool> accessing_camera,
                              std::optional<bool> accessing_microphone) {
    std::optional<bool> camera;
    std::optional<bool> microphone;
    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->AppCapabilityAccessCache()
        .ForOneApp(app_id, [&camera, &microphone](
                               const apps::CapabilityAccessUpdate& update) {
          camera = update.Camera();
          microphone = update.Microphone();
        });
    EXPECT_EQ(camera, accessing_camera);
    EXPECT_EQ(microphone, accessing_microphone);
  }

  void VerifyNoCapabilityAccess(const std::string& app_id) {
    ASSERT_FALSE(
        apps::AppServiceProxyFactory::GetForProfile(profile())
            ->AppCapabilityAccessCache()
            .ForOneApp(app_id, [](const apps::CapabilityAccessUpdate& update) {
              NOTREACHED();
            }));
  }
};

TEST_F(PublisherTest, ExtensionAppsOnApps) {
  // Re-init AppService to verify the init process.
  AppServiceTest app_service_test;
  app_service_test.SetUp(profile());

  // Install a "web store" app.
  scoped_refptr<extensions::Extension> store =
      MakeExtensionApp("webstore", "0.0", "http://google.com",
                       std::string(extensions::kWebStoreAppId));
  registrar()->AddExtension(store.get());

  VerifyApp(AppType::kChromeApp, store->id(), store->name(), Readiness::kReady,
            InstallReason::kDefault, InstallSource::kChromeWebStore, {},
            base::Time(), base::Time(), apps::Permissions(),
            /*is_platform_app=*/true, /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*allow_close=*/true,
            /*has_badge=*/false, /*paused=*/false,
            /*allow_window_mode_selection=*/std::nullopt);
  VerifyAppTypeIsInitialized(AppType::kChromeApp);

  // Uninstall the Chrome app.
  registrar()->UninstallExtension(
      store->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  VerifyApp(AppType::kChromeApp, store->id(), store->name(),
            Readiness::kUninstalledByUser, InstallReason::kDefault,
            InstallSource::kChromeWebStore, {}, base::Time(), base::Time(),
            apps::Permissions(),
            /*is_platform_app=*/true,
            /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*allow_close=*/true,
            /*has_badge=*/false, /*paused=*/false,
            /*allow_window_mode_selection=*/std::nullopt);

  // Reinstall the Chrome app.
  registrar()->AddExtension(store.get());
  VerifyApp(AppType::kChromeApp, store->id(), store->name(), Readiness::kReady,
            InstallReason::kDefault, InstallSource::kChromeWebStore, {},
            base::Time(), base::Time(), apps::Permissions(),
            /*is_platform_app=*/true, /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*allow_close=*/true,
            /*has_badge=*/false, /*paused=*/false,
            /*allow_window_mode_selection=*/std::nullopt);

  // Test OnExtensionLastLaunchTimeChanged.
  extensions::ExtensionPrefs::Get(profile())->SetLastLaunchTime(
      store->id(), kLastLaunchTime);
  VerifyApp(AppType::kChromeApp, store->id(), store->name(), Readiness::kReady,
            InstallReason::kDefault, InstallSource::kChromeWebStore, {},
            kLastLaunchTime, base::Time(), apps::Permissions(),
            /*is_platform_app=*/true);
}

TEST_F(PublisherTest, WebAppsOnApps) {
  const std::string kAppName = "Web App";
  AppServiceTest app_service_test_;
  app_service_test_.SetUp(profile());
  auto app_id = CreateWebApp(kAppName);

  InstallReason expected_install_reason = InstallReason::kUser;

  VerifyApp(AppType::kWeb, app_id, kAppName, Readiness::kReady,
            expected_install_reason, InstallSource::kBrowser, {}, base::Time(),
            base::Time(), apps::Permissions(),
            /*is_platform_app=*/false,
            /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*allow_close=*/true,
            /*has_badge=*/false, /*paused=*/false,
            /*allow_window_mode_selection=*/true, WindowMode::kWindow);
  VerifyIntentFilters(app_id);
  VerifyAppTypeIsInitialized(AppType::kWeb);
}

}  // namespace apps
