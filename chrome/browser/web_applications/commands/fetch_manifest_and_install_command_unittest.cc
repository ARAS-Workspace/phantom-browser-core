// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/command_metrics_test_helper.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_gmock_support.h"

namespace web_app {
namespace {

class FetchManifestAndInstallCommandTest
    : public WebAppTest
{
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");
  const webapps::AppId kWebAppId =
      GenerateAppId(/*manifest_id=*/std::nullopt, kWebAppUrl);
  const GURL kWebAppManifestUrl =
      GURL("https://example.com/path/manifest.json");
  const GURL kDefaultIconUrl = GURL("https://example.com/path/def_icon.png");
  const SkColor kDefaultIconColor = SK_ColorYELLOW;

  void SetUp() override {
    WebAppTest::SetUp();

    FakeWebAppProvider::Get(profile())->UseRealOsIntegrationManager();

    test::AwaitStartWebAppProviderAndSubsystems(profile());

    web_contents_manager().SetUrlLoaded(web_contents(), kWebAppUrl);

  }

  void TearDown() override {

    WebAppTest::TearDown();
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  FakeWebAppUiManager& fake_ui_manager() {
    return static_cast<FakeWebAppUiManager&>(fake_provider().ui_manager());
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  TestFileUtils& file_utils() {
    return *fake_provider().file_utils()->AsTestFileUtils();
  }

  WebAppInstallDialogCallback CreateDialogCallback(
      bool accept = true,
      mojom::UserDisplayMode user_display_mode =
          mojom::UserDisplayMode::kBrowser) {
    return base::BindOnce(
        [](bool accept, mojom::UserDisplayMode user_display_mode,
           base::WeakPtr<WebAppScreenshotFetcher>,
           content::WebContents* initiator_web_contents,
           std::unique_ptr<WebAppInstallInfo> web_app_info,
           WebAppInstallationAcceptanceCallback acceptance_callback) {
          web_app_info->user_display_mode = user_display_mode;
          std::move(acceptance_callback)
              .Run(accept, std::move(web_app_info),
                   base::BindOnce([](bool success,
                                     base::OnceClosure reparent_or_launch_app) {
                     if (success && reparent_or_launch_app) {
                       std::move(reparent_or_launch_app).Run();
                     }
                   }));
        },
        accept, user_display_mode);
  }

  blink::mojom::ManifestPtr CreateValidManifest() {
    blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
    manifest->name = u"foo";
    manifest->short_name = u"bar";
    manifest->start_url = kWebAppUrl;
    manifest->id = GenerateManifestIdFromStartUrlOnly(kWebAppUrl).value();
    manifest->display = blink::mojom::DisplayMode::kStandalone;
    blink::Manifest::ImageResource icon;
    icon.src = kDefaultIconUrl;
    icon.sizes = {{144, 144}};
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
    manifest->icons = {icon};
    return manifest;
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider()->web_contents_manager());
  }

  void SetupPageState(
      blink::mojom::ManifestPtr opt_manifest = blink::mojom::ManifestPtr()) {
    auto& page_state = web_contents_manager().GetOrCreatePageState(kWebAppUrl);

    page_state.has_service_worker = true;
    page_state.manifest_before_default_processing =
        opt_manifest ? std::move(opt_manifest) : CreateValidManifest();
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;

    // When using the default manifest, populate the default icon.
    if (!opt_manifest) {
      auto& icon_state =
          web_contents_manager().GetOrCreateIconState(kDefaultIconUrl);
      icon_state.bitmaps = {CreateSquareIcon(144, kDefaultIconColor)};
      icon_state.http_status_code = 200;
    }
  }

  void SetupIconState(IconsMap icons,
                      bool trigger_primary_page_changed = false,
                      int http_status_codes = 200) {
    for (const auto& [url, icon] : icons) {
      auto& icon_state = web_contents_manager().GetOrCreateIconState(url);
      icon_state.http_status_code = http_status_codes;
      icon_state.bitmaps = icon;
      icon_state.trigger_primary_page_changed_if_fetched =
          trigger_primary_page_changed;
    }
  }

  webapps::InstallResultCode InstallAndWait(
      webapps::WebappInstallSource install_surface,
      WebAppInstallDialogCallback dialog_callback,
      FallbackBehavior fallback_behavior =
          FallbackBehavior::kCraftedManifestOnly) {
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        install_future;
    provider()->scheduler().FetchManifestAndInstall(
        install_surface, web_contents()->GetWeakPtr(),
        std::move(dialog_callback), install_future.GetCallback(),
        fallback_behavior);
    EXPECT_TRUE(install_future.Wait());
    return install_future.Get<webapps::InstallResultCode>();
  }

 private:
  base::HistogramTester histogram_tester_;

};

TEST_F(FetchManifestAndInstallCommandTest, WebContentsDestroyed) {
  SetupPageState();

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  provider()->scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      web_contents()->GetWeakPtr(), CreateDialogCallback(),
      install_future.GetCallback(), FallbackBehavior::kCraftedManifestOnly);

  DeleteContents();
  ASSERT_TRUE(install_future.Wait());

  EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kWebContentsDestroyed);
}

TEST_F(FetchManifestAndInstallCommandTest, WebContentsNavigates) {
  SetupPageState();
  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      install_future;
  provider()->scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      web_contents()->GetWeakPtr(),
      CreateDialogCallback(/*accept=*/true,
                           mojom::UserDisplayMode::kStandalone),
      install_future.GetCallback(), FallbackBehavior::kCraftedManifestOnly);
  // The command is always started asynchronously, so this immediate
  // navigation should test that it correctly handles navigation before
  // starting.
  content::WebContentsTester* tester =
      content::WebContentsTester::For(web_contents());
  ASSERT_TRUE(tester);
  tester->NavigateAndCommit(GURL("https://other_origin.com/path/index.html"));
  ASSERT_TRUE(install_future.Wait());
  EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
  EXPECT_FALSE(
      provider()->registrar_unsafe().GetInstallState(kWebAppId).has_value());
}

}  // namespace
}  // namespace web_app
