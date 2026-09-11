// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/web_applications/link_capturing_features.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

#include "chrome/browser/ui/webui/app_management/web_app_settings_page_handler.h"
#include "chrome/common/chrome_features.h"

using ::testing::Contains;
using ::testing::ElementsAre;

namespace apps {
namespace {
class TestDelegate : public AppManagementPageHandlerBase::Delegate {
 public:
  TestDelegate() = default;
  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  // AppManagementPageHandlerBase::Delegate:

  ~TestDelegate() override = default;

  gfx::NativeWindow GetUninstallAnchorWindow() const override {
    return gfx::NativeWindow();
  }
};

enum class LaunchHandlerTest {
  kNavigateNew,
  kNavigateExisting,
  kFocusExisting,
  kUnset
};

using AppManagementPageHandlerWithUpdateStringsTestParams =
    std::tuple<apps::test::LinkCapturingFeatureVersion, LaunchHandlerTest>;

std::string LaunchHandlerTestParamsToString(
    const testing::TestParamInfo<
        AppManagementPageHandlerWithUpdateStringsTestParams>& info) {
  std::string version_str = apps::test::LinkCapturingVersionToString(
      testing::TestParamInfo<apps::test::LinkCapturingFeatureVersion>(
          std::get<0>(info.param), info.index));

  std::string launch_handler_str;
  switch (std::get<1>(info.param)) {
    case LaunchHandlerTest::kNavigateNew:
      launch_handler_str = "NavigateNew";
      break;
    case LaunchHandlerTest::kNavigateExisting:
      launch_handler_str = "NavigateExisting";
      break;
    case LaunchHandlerTest::kFocusExisting:
      launch_handler_str = "FocusExisting";
      break;
    case LaunchHandlerTest::kUnset:
      launch_handler_str = "Unset";
      break;
  }
  return version_str + "_" + launch_handler_str;
}
}  // namespace

class AppManagementPageHandlerTestBase
    : public WebAppTest,
      public testing::WithParamInterface<
          apps::test::LinkCapturingFeatureVersion> {
 public:
  AppManagementPageHandlerTestBase()
      : WebAppTest(WebAppTest::WithTestUrlLoaderFactory{}) {}

  void SetUp() override {
    WebAppTest::SetUp();

    delegate_ = std::make_unique<TestDelegate>();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    mojo::PendingReceiver<app_management::mojom::Page> page;
    mojo::Remote<app_management::mojom::PageHandler> handler;
    handler_ = std::make_unique<WebAppSettingsPageHandler>(
        handler.BindNewPipeAndPassReceiver(),
        page.InitWithNewPipeAndPassRemote(), profile(), *delegate_);
    auto features_and_params =
        apps::test::GetFeaturesToEnableLinkCapturingUX(GetParam());
    scoped_feature_list_.InitWithFeaturesAndParameters(features_and_params, {});
  }

  void TearDown() override {
    handler_.reset();
    WebAppTest::TearDown();
  }

  bool LinkCapturingEnabledByDefault() {
    return GetParam() == apps::test::LinkCapturingFeatureVersion::kV2DefaultOn;
  }

  AppManagementPageHandlerBase* handler() { return handler_.get(); }

 protected:
  void AwaitWebAppCommandsComplete() {
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForTest(profile());
    provider->command_manager().AwaitAllCommandsCompleteForTesting();
  }

  bool IsAppPreferred(const webapps::AppId& app_id) {
    base::test::TestFuture<app_management::mojom::AppPtr> result;
    handler()->GetApp(app_id, result.GetCallback());
    return result.Get()->is_preferred_app;
  }

  std::vector<std::string> GetOverlappingPreferredApps(
      const webapps::AppId& app_id) {
    base::test::TestFuture<const std::vector<std::string>&> result;
    handler()->GetOverlappingPreferredApps(app_id, result.GetCallback());
    EXPECT_TRUE(result.Wait());
    return result.Get();
  }

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<TestDelegate> delegate_;
  std::unique_ptr<AppManagementPageHandlerBase> handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(AppManagementPageHandlerTestBase, GetApp) {
  // Create a web app entry with scope, which would be recognised
  // as normal web app in the web app system.
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/"));
  web_app_info->title = u"app_name";

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  base::test::TestFuture<app_management::mojom::AppPtr> result;
  handler()->GetApp(app_id, result.GetCallback());

  EXPECT_EQ(result.Get()->id, app_id);
  EXPECT_EQ(result.Get()->title.value(), "app_name");
  EXPECT_EQ(result.Get()->type, AppType::kWeb);
}

TEST_P(AppManagementPageHandlerTestBase, GetPreferredAppTest) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/abc/index.html"));
  web_app_info->title = u"app_name";

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  if (LinkCapturingEnabledByDefault()) {
    EXPECT_TRUE(IsAppPreferred(app_id));
    ASSERT_EQ(test::DisableLinkCapturingByUser(profile(), app_id), base::ok());
  }
  EXPECT_FALSE(IsAppPreferred(app_id));

  handler()->SetPreferredApp(app_id, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  base::test::TestFuture<app_management::mojom::AppPtr> updated_result;
  handler()->GetApp(app_id, updated_result.GetCallback());
  EXPECT_TRUE(updated_result.Get()->is_preferred_app);

  EXPECT_THAT(updated_result.Get()->supported_links,
              testing::Contains("example.com/abc/*"));
}

TEST_P(AppManagementPageHandlerTestBase, DisablePreferredApp) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/abc/index.html"));
  web_app_info->title = u"app_name";

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  handler()->SetPreferredApp(app_id, /*is_preferred_app=*/false);
  AwaitWebAppCommandsComplete();

  base::test::TestFuture<app_management::mojom::AppPtr> updated_result;
  handler()->GetApp(app_id, updated_result.GetCallback());
  EXPECT_FALSE(updated_result.Get()->is_preferred_app);
}

TEST_P(AppManagementPageHandlerTestBase, SupportedLinksWithPort) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com:8080/abc/index.html"));
  web_app_info->title = u"app_name";

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  base::test::TestFuture<app_management::mojom::AppPtr> result;
  handler()->GetApp(app_id, result.GetCallback());

  EXPECT_THAT(result.Get()->supported_links,
              testing::Contains("example.com:8080/abc/*"));
}

TEST_P(AppManagementPageHandlerTestBase, PreferredAppNonOverlappingScopePort) {
  auto web_app_info1 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com:8080/index.html"));
  web_app_info1->title = u"App 1";

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  auto web_app_info2 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com:9090/index.html"));
  web_app_info2->title = u"App 2";

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));
  EXPECT_EQ(IsAppPreferred(app_id1), LinkCapturingEnabledByDefault());
  EXPECT_EQ(IsAppPreferred(app_id2), LinkCapturingEnabledByDefault());

  // app_id1 is set to preferred, app_id2 is not affected.
  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  EXPECT_TRUE(IsAppPreferred(app_id1));
  EXPECT_EQ(IsAppPreferred(app_id2), LinkCapturingEnabledByDefault());

  // app_id2 is set as preferred, app_id1 is not affected.
  handler()->SetPreferredApp(app_id2, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  EXPECT_TRUE(IsAppPreferred(app_id1));
  EXPECT_TRUE(IsAppPreferred(app_id2));
}

TEST_P(AppManagementPageHandlerTestBase, PreferredAppOverlappingScopePort) {
  auto web_app_info1 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com:8080/index.html"));
  web_app_info1->title = u"App 1";

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  auto web_app_info2 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com:8080/abc/index.html"));
  web_app_info2->title = u"App 2";

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));
  EXPECT_EQ(IsAppPreferred(app_id1), LinkCapturingEnabledByDefault());
  EXPECT_EQ(IsAppPreferred(app_id2), LinkCapturingEnabledByDefault());

  // Setting app_id1 as preferred should set app_id2 as not preferred.
  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  EXPECT_TRUE(IsAppPreferred(app_id1));
  EXPECT_EQ(IsAppPreferred(app_id2), LinkCapturingEnabledByDefault());

  handler()->SetPreferredApp(app_id2, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  EXPECT_TRUE(IsAppPreferred(app_id1));
  EXPECT_TRUE(IsAppPreferred(app_id2));
}

TEST_P(AppManagementPageHandlerTestBase,
       GetPreferredAppDifferentScopesNotReset) {
  // Install app1 and mark it as preferred.
  auto web_app_info1 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index.html"));
  web_app_info1->title = u"app_name";

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  // Install app2 with same scope as app1.
  auto web_app_info2 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index_abc.html"));
  web_app_info2->title = u"app_name2";

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // Install app3 with a completely different scope than app1 and app2.
  auto web_app_info3 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://abc.com/def/index.html"));
  web_app_info3->title = u"app_name3";

  std::string app_id3 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info3));

  // Set app2 and app3 as preferred
  handler()->SetPreferredApp(app_id2, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  handler()->SetPreferredApp(app_id3, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  // Verify the preferred app status of app_id1, app_id2 and app_id3. app_id1's
  // preferred app status should have been reset to false, while app_id2 and
  // app_id3 should still be true.
  EXPECT_FALSE(IsAppPreferred(app_id1));
  EXPECT_TRUE(IsAppPreferred(app_id2));
  EXPECT_TRUE(IsAppPreferred(app_id3));
}

TEST_P(AppManagementPageHandlerTestBase, GetPreferredAppTestInvalidAppId) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index.html"));
  web_app_info->title = u"app_name";

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  if (LinkCapturingEnabledByDefault()) {
    EXPECT_TRUE(IsAppPreferred(app_id));
    ASSERT_EQ(test::DisableLinkCapturingByUser(profile(), app_id), base::ok());
  }

  EXPECT_FALSE(IsAppPreferred(app_id));
  handler()->SetPreferredApp("def", /*is_preferred_app=*/true);
  EXPECT_FALSE(IsAppPreferred(app_id));
}

TEST_P(AppManagementPageHandlerTestBase,
       GetPreferredAppTestInvalidSupportedLink) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("chrome://settings/settingsapp/"));
  web_app_info->title = u"app_name";

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  if (LinkCapturingEnabledByDefault()) {
    EXPECT_TRUE(IsAppPreferred(app_id));
    ASSERT_EQ(test::DisableLinkCapturingByUser(profile(), app_id), base::ok());
  }

  EXPECT_FALSE(IsAppPreferred(app_id));

  handler()->SetPreferredApp(app_id, /*is_preferred_app=*/true);

  base::test::TestFuture<app_management::mojom::AppPtr> updated_result;
  handler()->GetApp(app_id, updated_result.GetCallback());
  EXPECT_FALSE(updated_result.Get()->is_preferred_app);
  EXPECT_TRUE(updated_result.Get()->supported_links.empty());
}

TEST_P(AppManagementPageHandlerTestBase,
       GetOverlappingPreferredAppsSingleAppOnly) {
  // First install an app that has some scope set in it.
  auto web_app_info1 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index.html"));
  web_app_info1->title = u"app_name";

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  // 2nd app has the same scope, but different app_id and opens in a new window.
  auto web_app_info2 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index_abc.html"));
  web_app_info2->title = u"app_name2";
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  if (LinkCapturingEnabledByDefault()) {
    EXPECT_TRUE(IsAppPreferred(app_id1));
    ASSERT_EQ(test::DisableLinkCapturingByUser(profile(), app_id1), base::ok());
  }

  // Set app_id1 as a preferred app.
  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  std::vector<std::string> overlapping_apps =
      GetOverlappingPreferredApps(app_id1);
  EXPECT_TRUE(overlapping_apps.empty());
}

TEST_P(AppManagementPageHandlerTestBase,
       GetOverlappingPreferredAppsNestedScope) {
  // First install an app that has some scope set in it.
  auto web_app_info1 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index.html"));
  web_app_info1->title = u"app_name";

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  // 2nd app has the same scope, but different app_id and opens in a new window.
  auto web_app_info2 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/nested/index_abc.html"));
  web_app_info2->title = u"app_name2";
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // Set app_id1 as a preferred app.
  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  std::vector<std::string> overlapping_apps =
      GetOverlappingPreferredApps(app_id2);

  EXPECT_TRUE(overlapping_apps.empty());
}

TEST_P(AppManagementPageHandlerTestBase, GetOverlappingPreferredAppsTwice) {
  // First install an app that has some scope set in it.
  auto web_app_info1 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index.html"));
  web_app_info1->title = u"app_name";

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  // 2nd app has the same scope, but different app_id and opens in a new window.
  auto web_app_info2 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index_abc.html"));
  web_app_info2->title = u"app_name2";
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // Set app_id1 as a preferred app.
  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  EXPECT_FALSE(IsAppPreferred(app_id2));

  std::vector<std::string> overlapping_apps =
      GetOverlappingPreferredApps(app_id1);
  EXPECT_TRUE(overlapping_apps.empty());

  // Set app_id2 as a preferred app.
  handler()->SetPreferredApp(app_id2, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  // app_id1 should be returned here.
  overlapping_apps = GetOverlappingPreferredApps(app_id1);
  EXPECT_THAT(overlapping_apps, testing::ElementsAre(app_id2));
}

TEST_P(AppManagementPageHandlerTestBase,
       GetOverlappingPreferredAppsTwiceNonPreferred) {
  // First install an app that has some scope set in it.
  auto web_app_info1 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index.html"));
  web_app_info1->title = u"app_name";

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  // 2nd app has the same scope, but different app_id and opens in a new window.
  auto web_app_info2 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index_abc.html"));
  web_app_info2->title = u"app_name2";
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // Set app_id1 as a preferred app.
  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  EXPECT_FALSE(IsAppPreferred(app_id2));

  std::vector<std::string> overlapping_apps =
      GetOverlappingPreferredApps(app_id1);
  EXPECT_TRUE(overlapping_apps.empty());

  // Set app_id2 as a preferred app.
  handler()->SetPreferredApp(app_id2, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  // Since app_id2 is already a preferred app, there should not be any other
  // preferred apps.
  overlapping_apps = GetOverlappingPreferredApps(app_id2);
  EXPECT_TRUE(overlapping_apps.empty());
}

TEST_P(AppManagementPageHandlerTestBase,
       GetOverlappingPreferredAppsShortcutApp) {
  auto web_app_info1 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index.html"));
  web_app_info1->title = u"app_name";

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  // The WebAppRegistrar treats an app without a scope as a shortcut app.
  auto web_app_info2 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index_abc.html"));
  web_app_info2->title = u"app_name2";
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // The result should be empty since app_id2 is a shortcut app.
  std::vector<std::string> overlapping_apps =
      GetOverlappingPreferredApps(app_id1);
  EXPECT_TRUE(overlapping_apps.empty());
}

TEST_P(AppManagementPageHandlerTestBase, DifferentScopeNoOverlap) {
  auto web_app_info1 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index.html"));
  web_app_info1->title = u"app_name";

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  auto web_app_info2 = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example_2.com/index_abc.html"));
  web_app_info2->title = u"app_name2";
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // The result should be empty since none of the apps have the same scope.
  std::vector<std::string> overlapping_apps =
      GetOverlappingPreferredApps(app_id1);
  EXPECT_TRUE(overlapping_apps.empty());
}

TEST_P(AppManagementPageHandlerTestBase, GetSupportedLinksWithScopeExtensions) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/"));
  web_app_info->title = u"app_name";
  web_app_info->scope_extensions = {
      web_app::ScopeExtensionInfo::CreateForScope(GURL("https://sitea.com")),
      web_app::ScopeExtensionInfo::CreateForScope(
          GURL("https://app.siteb.com")),
      web_app::ScopeExtensionInfo::CreateForScope(GURL("https://sitec.com"),
                                                  /*has_origin_wildcard=*/true),
      web_app::ScopeExtensionInfo::CreateForScope(
          GURL("https://sited.com/path")),
      web_app::ScopeExtensionInfo::CreateForScope(
          GURL("http://☃.net/")) /* Unicode */
  };
  web_app_info->validated_scope_extensions = web_app_info->scope_extensions;
  web_app_info->scope_extensions.insert(
      web_app::ScopeExtensionInfo::CreateForScope(
          GURL("https://unvalidatedscope.com")));

  web_app::WebAppInstallParams install_params;
  // Skip origin association validation for testing.
  install_params.skip_origin_association_validation = true;

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      future;
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(profile());
  provider->scheduler().InstallFromInfoWithParams(
      std::move(web_app_info), /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, future.GetCallback(),
      install_params);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            future.Get<webapps::InstallResultCode>());
  const webapps::AppId& app_id = future.Get<webapps::AppId>();

  base::test::TestFuture<app_management::mojom::AppPtr> result;
  handler()->GetApp(app_id, result.GetCallback());

  EXPECT_THAT(result.Get()->supported_links,
              testing::UnorderedElementsAre("sitea.com/*", "app.siteb.com/*",
                                            "*.sitec.com/*", "sitec.com/*",
                                            "sited.com/path*", "example.com/*",
                                            "xn--n3h.net/*"));
}

TEST_P(AppManagementPageHandlerTestBase, GetScopeExtensions) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/"));
  web_app_info->title = u"app_name";
  web_app_info->scope_extensions = web_app::ScopeExtensions({
      web_app::ScopeExtensionInfo::CreateForScope(GURL("https://sitea.com")),
      web_app::ScopeExtensionInfo::CreateForScope(
          GURL("https://app.siteb.com")),
      web_app::ScopeExtensionInfo::CreateForScope(GURL("https://sitec.com"),
                                                  /*has_origin_wildcard=*/true),
      web_app::ScopeExtensionInfo::CreateForScope(
          GURL("http://☃.net/")) /* Unicode */,
      web_app::ScopeExtensionInfo::CreateForScope(
          GURL("https://localhost:443")),
      web_app::ScopeExtensionInfo::CreateForScope(
          GURL("https://localhost:9999")),
      web_app::ScopeExtensionInfo::CreateForScope(
          GURL("https://google.com/search?q=search+query")),
      web_app::ScopeExtensionInfo::CreateForScope(
          GURL("https://google.com/search?q=search+query#fragment")),
  });

  web_app::WebAppInstallParams install_params;
  // Skip origin association validation for testing.
  install_params.skip_origin_association_validation = true;

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      future;
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(profile());
  provider->scheduler().InstallFromInfoWithParams(
      std::move(web_app_info), /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, future.GetCallback(),
      install_params);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            future.Get<webapps::InstallResultCode>());
  const webapps::AppId& app_id = future.Get<webapps::AppId>();

  base::test::TestFuture<app_management::mojom::AppPtr> result;
  handler()->GetApp(app_id, result.GetCallback());

  std::vector<std::string> expected_scope_extensions = {
      "xn--n3h.net" /* Unicode */,
      "app.siteb.com",
      "google.com",
      "localhost",
      "sitea.com",
      "*.sitec.com",
      "localhost:9999"};
  EXPECT_EQ(result.Get()->scope_extensions, expected_scope_extensions);
}

// TODO(crbug.com/40279851): The overlapping nested scope based behavior is only
// on will need to be modified if the behavior changes.

TEST_P(AppManagementPageHandlerTestBase, NavigationCapturingUserChoice) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index.html"));
  web_app_info->title = u"app_name";
  web_app_info->user_display_mode = web_app::mojom::UserDisplayMode::kBrowser;

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  base::test::TestFuture<app_management::mojom::AppPtr> app_future;
  handler()->GetApp(app_id, app_future.GetCallback());
  bool expected_value = true;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  expected_value =
      base::FeatureList::IsEnabled(apps::features::kUpdateAppStringsOnSettings);
#endif
  EXPECT_EQ(app_future.Get()->disable_user_choice_navigation_capturing,
            expected_value);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AppManagementPageHandlerTestBase,
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOn),
    apps::test::LinkCapturingVersionToString);

// Tests that when `kUpdateAppStringsOnSettings` is enabled, browser-tab web
// apps have their user choice navigation capturing disabled or enabled
// depending on their launch handler settings.
class AppManagementPageHandlerWithUpdateStringsTest
    : public WebAppTest,
      public testing::WithParamInterface<
          AppManagementPageHandlerWithUpdateStringsTestParams> {
 public:
  AppManagementPageHandlerWithUpdateStringsTest() = default;

  void SetUp() override {
    WebAppTest::SetUp();

    delegate_ = std::make_unique<TestDelegate>();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    mojo::PendingReceiver<app_management::mojom::Page> page;
    mojo::Remote<app_management::mojom::PageHandler> handler;
    std::vector<base::test::FeatureRefAndParams> features_and_params;
    features_and_params =
        apps::test::GetFeaturesToEnableLinkCapturingUX(std::get<0>(GetParam()));
    features_and_params.push_back(base::test::FeatureRefAndParams(
        apps::features::kUpdateAppStringsOnSettings, {}));
    scoped_feature_list_.InitWithFeaturesAndParameters(features_and_params, {});

    handler_ = std::make_unique<WebAppSettingsPageHandler>(
        handler.BindNewPipeAndPassReceiver(),
        page.InitWithNewPipeAndPassRemote(), profile(), *delegate_);
  }

  void TearDown() override {
    handler_.reset();
    scoped_feature_list_.Reset();
    WebAppTest::TearDown();
  }

 protected:
  app_management::mojom::PageHandler* handler() { return handler_.get(); }

 private:
  std::unique_ptr<TestDelegate> delegate_;
  std::unique_ptr<app_management::mojom::PageHandler> handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(AppManagementPageHandlerWithUpdateStringsTest,
       NavigationCapturingUserChoice) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/index.html"));
  web_app_info->title = u"app_name";
  web_app_info->user_display_mode = web_app::mojom::UserDisplayMode::kBrowser;

  LaunchHandlerTest launch_handler_test = std::get<1>(GetParam());
  switch (launch_handler_test) {
    case LaunchHandlerTest::kNavigateNew:
      web_app_info->launch_handler = web_app::LaunchHandler{
          web_app::LaunchHandler::ClientMode::kNavigateNew};
      break;
    case LaunchHandlerTest::kNavigateExisting:
      web_app_info->launch_handler = web_app::LaunchHandler{
          web_app::LaunchHandler::ClientMode::kNavigateExisting};
      break;
    case LaunchHandlerTest::kFocusExisting:
      web_app_info->launch_handler = web_app::LaunchHandler{
          web_app::LaunchHandler::ClientMode::kFocusExisting};
      break;
    case LaunchHandlerTest::kUnset:
      break;
  }

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  base::test::TestFuture<app_management::mojom::AppPtr> app_future;
  handler()->GetApp(app_id, app_future.GetCallback());
  bool should_link_capturing_setting_be_disabled =
      launch_handler_test == LaunchHandlerTest::kNavigateNew ||
      launch_handler_test == LaunchHandlerTest::kUnset;
  EXPECT_EQ(app_future.Get()->disable_user_choice_navigation_capturing,
            should_link_capturing_setting_be_disabled);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AppManagementPageHandlerWithUpdateStringsTest,
    testing::Combine(
        testing::Values(apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                        apps::test::LinkCapturingFeatureVersion::kV2DefaultOn),
        testing::Values(LaunchHandlerTest::kNavigateNew,
                        LaunchHandlerTest::kNavigateExisting,
                        LaunchHandlerTest::kFocusExisting,
                        LaunchHandlerTest::kUnset)),
    LaunchHandlerTestParamsToString);

}  // namespace apps
