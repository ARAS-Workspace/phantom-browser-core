// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_silent_update_command.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/scheduler/manifest_silent_update_result.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/sync/protocol/web_app_specifics.equal.h"
#include "components/sync/protocol/web_app_specifics.ostream.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_gmock_support.h"

namespace web_app {

class ManifestSilentUpdateCommandTest : public WebAppTest {
 public:
  const GURL kDefaultIconUrl = GURL("https://example.com/path/def_icon.png");
  const GURL kAppUrl = GURL("https://www.foo.bar/web_apps/basic.html");
  static constexpr SkColor kManifestIconColor = SK_ColorCYAN;
  static constexpr int kManifestIconSize = 96;

  ManifestSilentUpdateCommandTest() = default;
  ManifestSilentUpdateCommandTest(const ManifestSilentUpdateCommandTest&) =
      delete;
  ManifestSilentUpdateCommandTest& operator=(
      const ManifestSilentUpdateCommandTest&) = delete;
  ~ManifestSilentUpdateCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->SetOriginAssociationManager(
        std::make_unique<FakeWebAppOriginAssociationManager>(*profile()));
    provider->StartWithSubsystems();
    test::WaitUntilWebAppProviderAndSubsystemsReady(provider);

    web_contents_manager().SetUrlLoaded(web_contents(), kAppUrl);
  }

 protected:
  void SetupBasicInstallablePageState() {
    auto& page_state = web_contents_manager().GetOrCreatePageState(kAppUrl);

    page_state.manifest_url = GURL("https://www.example.com/manifest.json");
    page_state.has_service_worker = false;
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;

    // Set up manifest icon.
    blink::Manifest::ImageResource icon;
    icon.src = kDefaultIconUrl;
    icon.sizes = {{kManifestIconSize, kManifestIconSize}};
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

    // Set icons in content.
    web_contents_manager().GetOrCreateIconState(kDefaultIconUrl).bitmaps = {
        gfx::test::CreateBitmap(kManifestIconSize, kManifestIconColor)};

    // Set up manifest.
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = kAppUrl;
    manifest->id = GenerateManifestIdFromStartUrlOnly(kAppUrl).value();
    manifest->scope = kAppUrl.GetWithoutFilename();
    manifest->display = DisplayMode::kStandalone;
    manifest->name = u"Foo App";
    manifest->icons = {icon};
    manifest->background_color = kManifestIconColor;
    manifest->theme_color = kManifestIconColor;
    manifest->has_valid_specified_start_url = true;
    auto note_taking = blink::mojom::ManifestNoteTaking::New();
    note_taking->new_note_url = GURL("https://www.foo.bar/web_apps/new_note");
    manifest->note_taking = std::move(note_taking);
    manifest->launch_handler = LaunchHandler(
        blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateNew);

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  ManifestSilentUpdateCheckResult RunManifestUpdateAndGetResult(
      std::optional<base::Time> previous_time_for_silent_icon_update =
          std::nullopt) {
    base::test::TestFuture<ManifestSilentUpdateCompletionInfo>
        manifest_silent_update_future;
    fake_provider().scheduler().ScheduleManifestSilentUpdate(
        *web_contents(), previous_time_for_silent_icon_update,
        manifest_silent_update_future.GetCallback());

    EXPECT_TRUE(manifest_silent_update_future.Wait());
    return manifest_silent_update_future.Take().result;
  }

  blink::mojom::ManifestPtr& GetPageManifest() {
    return web_contents_manager()
        .GetOrCreatePageState(kAppUrl)
        .manifest_before_default_processing;
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  TestFileUtils& file_utils() {
    return *fake_provider().file_utils()->AsTestFileUtils();
  }

  bool AppHasPendingUpdateInfo(const webapps::AppId& app_id) {
    return provider()
        .registrar_unsafe()
        .GetAppById(app_id)
        ->pending_update_info()
        .has_value();
  }

  base::FilePath GetAppPendingTrustedIconsDir(Profile* profile,
                                              const webapps::AppId& app_id) {
    base::FilePath web_apps_root_directory = GetWebAppsRootDirectory(profile);
    base::FilePath app_dir =
        GetManifestResourcesDirectoryForApp(web_apps_root_directory, app_id);
    return app_dir.AppendASCII("Pending Trusted Icons");
  }

  base::FilePath GetAppPendingManifestIconsDir(Profile* profile,
                                               const webapps::AppId& app_id) {
    base::FilePath web_apps_root_directory = GetWebAppsRootDirectory(profile);
    base::FilePath app_dir =
        GetManifestResourcesDirectoryForApp(web_apps_root_directory, app_id);
    return app_dir.AppendASCII("Pending Manifest Icons");
  }

  std::vector<int> GetStoredIconSizesForPurpose(
      const google::protobuf::RepeatedPtrField<proto::DownloadedIconSizeInfo>&
          downloaded_icons,
      sync_pb::WebAppIconInfo_Purpose purpose) {
    for (const auto& info : downloaded_icons) {
      if (info.purpose() == purpose) {
        const auto& sizes_field = info.icon_sizes();
        return std::vector<int>(sizes_field.begin(), sizes_field.end());
      }
    }
    return {};
  }

  base::HistogramTester histogram_tester_;
};

TEST_F(ManifestSilentUpdateCommandTest, AppNotInstalledNotSilentlyUpdated) {
  SetupBasicInstallablePageState();

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate,
                  /*count=*/1)));
}

// TODO(crbug.com/424246884): Check for lock_screen_start_url to update if the
// feature is enabled.

TEST_F(ManifestSilentUpdateCommandTest, SyncInstalledAppUpdated) {
  webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Name", GURL("https://www.foo.bar/web_apps/basic.html"),
      webapps::WebappInstallSource::SYNC);
  SetupBasicInstallablePageState();

  auto& new_manifest = GetPageManifest();
  new_manifest->name = u"New Name";
  new_manifest->theme_color = SK_ColorYELLOW;

  // Sync installed apps should allow updates, but security sensitive updates
  // (like name) should still be pending. Non-sensitive updates (like theme
  // color) should be applied.
  EXPECT_EQ(
      RunManifestUpdateAndGetResult(),
      ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges);

  EXPECT_EQ(provider().registrar_unsafe().GetAppThemeColor(app_id),
            SK_ColorYELLOW);

  std::optional<proto::PendingUpdateInfo> pending_update_info =
      provider().registrar_unsafe().GetAppById(app_id)->pending_update_info();
  ASSERT_TRUE(pending_update_info.has_value());
  EXPECT_EQ(pending_update_info->name(), "New Name");

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(base::Bucket(
          ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges,
          /*count=*/1)));
}

TEST_F(ManifestSilentUpdateCommandTest, NoIconToIcons) {
  // Install via WebAppInstallInfo, with no icons and OS integration, to full
  // icons.
  auto web_app_install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(kAppUrl);
  web_app_install_info->title = u"A Basic Web App";
  web_app_install_info->display_mode = DisplayMode::kStandalone;
  web_app_install_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  web_app_install_info->is_generated_icon = true;
  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_install_info));

  SetupBasicInstallablePageState();
  ManifestSilentUpdateCheckResult result = RunManifestUpdateAndGetResult();
  EXPECT_TRUE(IsAppUpdated(result));
}

TEST_F(ManifestSilentUpdateCommandTest, OpenInBrowserTabUpdate) {
  auto web_app_install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(kAppUrl);
  web_app_install_info->title = u"A Basic Web App";
  web_app_install_info->display_mode = DisplayMode::kBrowser;
  web_app_install_info->user_display_mode = mojom::UserDisplayMode::kBrowser;
  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_install_info));

  SetupBasicInstallablePageState();
  ManifestSilentUpdateCheckResult result = RunManifestUpdateAndGetResult();
  EXPECT_TRUE(IsAppUpdated(result));
}

class ManifestSilentUpdateCommandExternalAppsTest
    : public ManifestSilentUpdateCommandTest,
      public testing::WithParamInterface<ExternalInstallSource> {
 public:
  webapps::AppId InstallExternallyManagedAppFromSource() {
    // This will always install external apps that open in a new browser tab.
    ExternalInstallOptions install_options(kAppUrl,
                                           /*user_display_mode=*/std::nullopt,
                                           GetParam());

    base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
    provider().scheduler().InstallExternallyManagedApp(
        install_options,
        /*installed_placeholder_app_id=*/std::nullopt, future.GetCallback());
    const ExternallyManagedAppManager::InstallResult& result =
        future.Get<ExternallyManagedAppManager::InstallResult>();
    EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);
    EXPECT_TRUE(result.app_id.has_value());
    return *result.app_id;
  }
};

TEST_P(ManifestSilentUpdateCommandExternalAppsTest, AppUpToDate) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppUpToDate);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(base::Bucket(ManifestSilentUpdateCheckResult::kAppUpToDate,
                              /*count=*/1)));
}

TEST_P(ManifestSilentUpdateCommandExternalAppsTest, StartUrlUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            "https://www.foo.bar/web_apps/basic.html");

  auto& new_manifest = GetPageManifest();
  const GURL new_start_url("https://www.foo.bar/new_scope/new_basic.html");
  new_manifest->start_url = new_start_url;

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id),
            new_start_url);

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_P(ManifestSilentUpdateCommandExternalAppsTest,
       AppNameChangedNoPendingUpdateInfoSaved) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->untranslated_name(),
      base::UTF16ToUTF8(u"Foo App"));

  auto& new_manifest = GetPageManifest();
  new_manifest->name = u"New Name";

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->untranslated_name(),
      base::UTF16ToUTF8(u"New Name"));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_P(ManifestSilentUpdateCommandExternalAppsTest,
       IconMoreThanTenPercentDiffChangedUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);

  auto& new_manifest = GetPageManifest();

  // Set up manifest icon.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{96, 96}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

  new_manifest->icons = {new_icon};

  // Set icon in content. Setting the icon color to YELLOW to trigger a more
  // than 10% image diff.
  SkBitmap updated_bitmap = gfx::test::CreateBitmap(96, SK_ColorYELLOW);
  web_contents_manager()
      .GetOrCreateIconState(GURL("https://example2.com/path/def_icon.png"))
      .bitmaps = {updated_bitmap};

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));

  // Verify pending update icon bitmaps are not written to disk.
  EXPECT_FALSE(
      base::PathExists(GetAppPendingTrustedIconsDir(profile(), app_id)));
  EXPECT_FALSE(
      base::PathExists(GetAppPendingManifestIconsDir(profile(), app_id)));

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_P(ManifestSilentUpdateCommandExternalAppsTest,
       IconLessThanTenPercentChangedDiffUpdatedSilently) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);

  auto& new_manifest = GetPageManifest();

  // Set up manifest icon.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{96, 96}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

  new_manifest->icons = {new_icon};

  SkBitmap changed_bitmap = gfx::test::CreateBitmap(96, SK_ColorCYAN);
  // For a 96x96 image, total pixels = 9216.
  // 10% of 9216 = 921.6 pixels.
  // We'll change a small area, for example, the first 9 rows, to a different
  // color. 9 rows * 96 columns = 864 pixels changed. This is < 10%.
  changed_bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 96, 9), SK_ColorRED);

  // Set icon in content.
  web_contents_manager()
      .GetOrCreateIconState(GURL("https://example2.com/path/def_icon.png"))
      .bitmaps = {changed_bitmap};

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));

  // Verify pending update icon bitmaps are not saved to disk.
  EXPECT_FALSE(
      base::PathExists(GetAppPendingTrustedIconsDir(profile(), app_id)));
  EXPECT_FALSE(
      base::PathExists(GetAppPendingManifestIconsDir(profile(), app_id)));

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Webapp.Update.ManifestSilentUpdateCheckResult"),
              BucketsAre(base::Bucket(
                  ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                  /*count=*/1)));
}

TEST_P(ManifestSilentUpdateCommandExternalAppsTest,
       DoubleVisitsUpdateOnlyOnce) {
  SetupBasicInstallablePageState();
  webapps::AppId app_id = InstallExternallyManagedAppFromSource();

  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example.com/path/def_icon.png"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).size(), 1u);

  auto& new_manifest = GetPageManifest();

  // Set up manifest icon for the first visit.
  blink::Manifest::ImageResource new_icon;
  new_icon.src = GURL("https://example2.com/path/def_icon.png");
  new_icon.sizes = {{96, 96}};
  new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  new_manifest->icons = {new_icon};

  // Set icon in content. Setting the icon color to YELLOW to trigger a more
  // than 10% image diff.
  SkBitmap updated_bitmap = gfx::test::CreateBitmap(96, SK_ColorYELLOW);
  web_contents_manager()
      .GetOrCreateIconState(GURL("https://example2.com/path/def_icon.png"))
      .bitmaps = {updated_bitmap};

  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);
  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));

  // Retrigger a manifest update for the same set of constraints, and verify
  // that an update does not happen.
  EXPECT_EQ(RunManifestUpdateAndGetResult(),
            ManifestSilentUpdateCheckResult::kAppUpToDate);
  ASSERT_FALSE(AppHasPendingUpdateInfo(app_id));
  EXPECT_EQ(provider().registrar_unsafe().GetAppIconInfos(app_id).begin()->url,
            GURL("https://example2.com/path/def_icon.png"));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          "Webapp.Update.ManifestSilentUpdateCheckResult"),
      BucketsAre(
          base::Bucket(ManifestSilentUpdateCheckResult::kAppSilentlyUpdated,
                       /*count=*/1),
          base::Bucket(ManifestSilentUpdateCheckResult::kAppUpToDate,
                       /*count=*/1)));
}

INSTANTIATE_TEST_SUITE_P(
    AllExternalInstallSources,
    ManifestSilentUpdateCommandExternalAppsTest,
    testing::Values(ExternalInstallSource::kInternalDefault,
                    ExternalInstallSource::kExternalDefault,
                    ExternalInstallSource::kExternalPolicy),
    [](const testing::TestParamInfo<ExternalInstallSource>& info) {
      switch (info.param) {
        case ExternalInstallSource::kInternalDefault:
          return "InternalDefault";
        case ExternalInstallSource::kExternalDefault:
          return "ExternalDefault";
        case ExternalInstallSource::kExternalPolicy:
          return "ExternalPolicy";
        default:
          NOTREACHED();
      }
    });

}  // namespace web_app
