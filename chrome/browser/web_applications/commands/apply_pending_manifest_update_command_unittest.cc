// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/apply_pending_manifest_update_command.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/scheduler/apply_pending_manifest_update_result.h"
#include "chrome/browser/web_applications/scheduler/manifest_silent_update_result.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_gmock_support.h"
#include "url/origin.h"

namespace web_app {
namespace {

constexpr int kAppIconSize = 10;
constexpr int kUpdatedAppIconSize = 56;
constexpr SkColor kAppIconColor = SK_ColorCYAN;
constexpr SkColor kUpdatedAppIconColor = SK_ColorYELLOW;

class ApplyPendingManifestUpdateCommandTest : public WebAppTest {
 public:
  ApplyPendingManifestUpdateCommandTest() = default;
  ApplyPendingManifestUpdateCommandTest(
      const ApplyPendingManifestUpdateCommandTest&) = delete;
  ApplyPendingManifestUpdateCommandTest& operator=(
      const ApplyPendingManifestUpdateCommandTest&) = delete;
  ~ApplyPendingManifestUpdateCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->UseRealOsIntegrationManager();
    provider->StartWithSubsystems();
    test::WaitUntilWebAppProviderAndSubsystemsReady(provider);
  }

 protected:
  void SetupBasicInstallablePageState() {
    const GURL default_icon_url{"https://example.com/path/def_icon.png"};

    web_contents_manager().SetUrlLoaded(web_contents(), kAppUrl);
    auto& page_state = web_contents_manager().GetOrCreatePageState(kAppUrl);

    page_state.manifest_url = GURL("https://www.example.com/manifest.json");
    page_state.has_service_worker = false;
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;

    // Set up manifest icon.
    blink::Manifest::ImageResource icon;
    icon.src = default_icon_url;
    icon.sizes = {{kAppIconSize, kAppIconSize}};
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

    // Set icons in content.
    web_contents_manager().GetOrCreateIconState(default_icon_url).bitmaps = {
        gfx::test::CreateBitmap(kAppIconSize, kAppIconColor)};

    // Set up manifest.
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = kAppUrl;
    manifest->id = GenerateManifestIdFromStartUrlOnly(kAppUrl).value();
    manifest->name = u"Foo App";
    manifest->icons = {icon};
    manifest->has_valid_specified_start_url = true;

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  blink::mojom::ManifestPtr& GetPageManifest() {
    return web_contents_manager()
        .GetOrCreatePageState(kAppUrl)
        .manifest_before_default_processing;
  }

  ManifestSilentUpdateCheckResult RunManifestSilentUpdateAndGetResult() {
    base::test::TestFuture<ManifestSilentUpdateCompletionInfo>
        manifest_silent_update_future;
    fake_provider().scheduler().ScheduleManifestSilentUpdate(
        *web_contents(), /*previous_time_for_silent_icon_update=*/std::nullopt,
        manifest_silent_update_future.GetCallback());

    EXPECT_TRUE(manifest_silent_update_future.Wait());
    return manifest_silent_update_future.Take().result;
  }

  ApplyPendingManifestUpdateResult RunManifestApplyPendingUpdateAndGetResult(
      const webapps::AppId& app_id) {
    base::test::TestFuture<ApplyPendingManifestUpdateResult>
        manifest_apply_pending_update_future;
    fake_provider().scheduler().ScheduleApplyPendingManifestUpdate(
        app_id, /*keep_alive=*/nullptr, /*profile_keep_alive=*/nullptr,
        manifest_apply_pending_update_future.GetCallback());

    EXPECT_TRUE(manifest_apply_pending_update_future.Wait());
    return manifest_apply_pending_update_future.Get();
  }

  SkBitmap ChangePageIconAndGetBitmap(blink::mojom::ManifestPtr& manifest) {
    blink::Manifest::ImageResource new_icon;
    const GURL new_icon_url = GURL("https://example2.com/path/def_icon.png");
    new_icon.src = new_icon_url;
    new_icon.sizes = {{kUpdatedAppIconSize, kUpdatedAppIconSize}};
    new_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
    manifest->icons = {new_icon};

    // Set icon in content. Setting the icon color to YELLOW to trigger a more
    // than 10% image diff to create a pending update info when updating.
    SkBitmap updated_bitmap =
        gfx::test::CreateBitmap(kUpdatedAppIconSize, kUpdatedAppIconColor);
    web_contents_manager().GetOrCreateIconState(new_icon_url).bitmaps = {
        updated_bitmap};
    return updated_bitmap;
  }

  using WebAppBitmaps = WebAppIconManager::WebAppBitmaps;
  WebAppBitmaps ReadIconBitmapsFromIconManager(const webapps::AppId& app_id) {
    base::test::TestFuture<WebAppIconManager::WebAppBitmaps>
        read_all_icons_future;
    provider().icon_manager().ReadAllIcons(app_id,
                                           read_all_icons_future.GetCallback());
    return read_all_icons_future.Get();
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

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebApps};
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::HistogramTester histogram_tester_;
  const GURL kAppUrl = GURL("https://www.foo.bar/web_apps/basic.html");
};

}  // namespace

TEST_F(ApplyPendingManifestUpdateCommandTest, VerifyIwaSubAppLogsUkmAndUma) {
  auto iwa = IsolatedWebAppBuilder(ManifestBuilder().SetName("Parent App"))
                 .BuildBundle();
  iwa->TrustSigningKey();
  iwa->FakeInstallPageState(profile());
  ASSERT_OK_AND_ASSIGN(auto url_info, iwa->Install(profile()));
  webapps::AppId parent_app_id = url_info.app_id();
  GURL parent_url = url_info.origin().GetURL();

  auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      parent_url.Resolve("/sub-app"));
  install_info->parent_app_id = parent_app_id;
  install_info->title = u"Sub App";
  webapps::AppId sub_app_id =
      test::InstallWebApp(profile(), std::move(install_info));
  GURL sub_app_url = parent_url.Resolve("/sub-app");

  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app = update->UpdateApp(sub_app_id);

    // Manually set pending update info.
    proto::PendingUpdateInfo pending_update_info;
    pending_update_info.set_name("New Name");
    pending_update_info.set_was_ignored(false);
    app->SetPendingUpdateInfo(std::move(pending_update_info));
  }

  // Verify that it is indeed recognized as IWA sub-app.
  ASSERT_TRUE(provider().registrar_unsafe().AppMatches(
      sub_app_id, WebAppFilter::IsIsolatedSubApp()));
  ASSERT_TRUE(provider()
                  .registrar_unsafe()
                  .GetAppById(sub_app_id)
                  ->pending_update_info());

  // Setup UKM recorder.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  EXPECT_EQ(RunManifestApplyPendingUpdateAndGetResult(sub_app_id),
            ApplyPendingManifestUpdateResult::kAppNameUpdatedSuccessfully);

  // Verify UMA IS logged.
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "WebApp.Update.ApplyPendingManifestUpdateResult"),
              BucketsAre(base::Bucket(
                  ApplyPendingManifestUpdateResult::kAppNameUpdatedSuccessfully,
                  /*count=*/1)));

  // Verify UKM IS logged.
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::SubApp_Update_ApplyPendingManifestUpdateResult::
          kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  const auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::SubApp_Update_ApplyPendingManifestUpdateResult::
          kResultName,
      static_cast<int>(
          ApplyPendingManifestUpdateResult::kAppNameUpdatedSuccessfully));

  // The UKM metric is send per parent app url.
  test_ukm_recorder.ExpectEntrySourceHasUrl(
      entry, url::Origin::Create(sub_app_url).GetURL());
}

}  // namespace web_app
