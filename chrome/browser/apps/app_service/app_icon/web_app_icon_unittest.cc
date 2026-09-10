// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/app_service/app_icon/web_app_icon_test_helper.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace apps {

using IconPurpose = web_app::IconPurpose;

class WebAppIconFactoryTest : public testing::Test {
 public:
  WebAppIconFactoryTest() = default;

  ~WebAppIconFactoryTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    TestingProfile::Builder builder;
    profile_ = builder.Build();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  gfx::ImageSkia LoadIconFromWebApp(const std::string& app_id,
                                    apps::IconEffects icon_effects) {
    base::test::TestFuture<apps::IconValuePtr> future;
    apps::LoadIconFromWebApp(profile(), apps::IconType::kStandard, kSizeInDip,
                             app_id, icon_effects, future.GetCallback());
    auto icon = future.Take();
    EnsureRepresentationsLoaded(icon->uncompressed);
    return icon->uncompressed;
  }

  apps::IconValuePtr LoadCompressedIconBlockingFromWebApp(
      const std::string& app_id,
      apps::IconEffects icon_effects) {
    base::test::TestFuture<apps::IconValuePtr> future;
    apps::LoadIconFromWebApp(profile(), apps::IconType::kCompressed, kSizeInDip,
                             app_id, icon_effects, future.GetCallback());
    auto icon = future.Take();
    return icon;
  }

  web_app::WebAppIconManager& icon_manager() {
    return web_app_provider().icon_manager();
  }

  web_app::WebAppProvider& web_app_provider() {
    return *web_app::WebAppProvider::GetForWebApps(profile());
  }

  web_app::WebAppSyncBridge& sync_bridge() {
    return web_app_provider().sync_bridge_unsafe();
  }

  Profile* profile() { return profile_.get(); }

  WebAppIconTestHelper test_helper() { return WebAppIconTestHelper(profile()); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(WebAppIconFactoryTest, LoadNonMaskableIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  gfx::ImageSkia src_image_skia =
      test_helper().GenerateWebAppIcon(app_id, IconPurpose::ANY, sizes_px,
                                       {{1.0, kIconSize1}, {2.0, kIconSize2}});

  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;

  icon_effect |= apps::IconEffects::kCrOsStandardIcon;

  gfx::ImageSkia dst = LoadIconFromWebApp(app_id, icon_effect);

  VerifyIcon(src_image_skia, dst);
}

TEST_F(WebAppIconFactoryTest, LoadNonMaskableNonEffectCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = kSizeInDip;
  const int kIconSize2 = 128;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      {{1.0, kIconSize1}, {2.0, kIconSize2}}, /*scale=*/1.0);

  auto icon =
      LoadCompressedIconBlockingFromWebApp(app_id, apps::IconEffects::kNone);

  VerifyCompressedIcon(src_data, *icon);
}

TEST_F(WebAppIconFactoryTest,
       LoadNonMaskableNonEffectCompressedIconWithDifferentSizeIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      {{1.0, kIconSize1}, {2.0, kIconSize2}}, /*scale=*/1.0);

  auto icon =
      LoadCompressedIconBlockingFromWebApp(app_id, apps::IconEffects::kNone);

  VerifyCompressedIcon(src_data, *icon);
}

TEST_F(WebAppIconFactoryTest, LoadNonMaskableCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, sizes_px,
      {{1.0, kIconSize1}, {2.0, kIconSize2}});

  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;
  icon_effect |= apps::IconEffects::kCrOsStandardIcon;

  auto icon = LoadCompressedIconBlockingFromWebApp(app_id, icon_effect);

  VerifyCompressedIcon(src_data, *icon);
}

TEST_F(WebAppIconFactoryTest, LoadMaskableIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE},
                           sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(
      icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, {kIconSize2}));

  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      app_id, IconPurpose::MASKABLE, {kIconSize2},
      {{1.0, kIconSize2}, {2.0, kIconSize2}});

  gfx::ImageSkia dst = LoadIconFromWebApp(
      app_id, apps::IconEffects::kRoundCorners |
                  apps::IconEffects::kCrOsStandardBackground |
                  apps::IconEffects::kCrOsStandardMask);
  VerifyIcon(src_image_skia, dst);
}

TEST_F(WebAppIconFactoryTest, LoadMaskableCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE},
                           sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  test_helper().RegisterApp(std::move(web_app));

  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;
  apps::IconValuePtr icon;

  icon_effect |= apps::IconEffects::kCrOsStandardBackground |
                 apps::IconEffects::kCrOsStandardMask;
  ASSERT_TRUE(
      icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, {kIconSize2}));

  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::MASKABLE, {kIconSize2},
      {{1.0, kIconSize2}, {2.0, kIconSize2}});

  icon = LoadCompressedIconBlockingFromWebApp(app_id, icon_effect);

  VerifyCompressedIcon(src_data, *icon);
}

TEST_F(WebAppIconFactoryTest, LoadNonMaskableIconWithMaskableIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 96;
  const int kIconSize2 = 128;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE},
                           sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize2});

  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, {kIconSize2}));

  gfx::ImageSkia src_image_skia =
      test_helper().GenerateWebAppIcon(app_id, IconPurpose::ANY, {kIconSize2},
                                       {{1.0, kIconSize2}, {2.0, kIconSize2}});

  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;

  icon_effect |= apps::IconEffects::kCrOsStandardIcon;

  gfx::ImageSkia dst = LoadIconFromWebApp(app_id, icon_effect);

  VerifyIcon(src_image_skia, dst);
}

TEST_F(WebAppIconFactoryTest, LoadSmallMaskableIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE},
                           sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, sizes_px));

  gfx::ImageSkia src_image_skia =
      test_helper().GenerateWebAppIcon(app_id, IconPurpose::MASKABLE, sizes_px,
                                       {{1.0, kIconSize1}, {2.0, kIconSize1}});

  gfx::ImageSkia dst = LoadIconFromWebApp(
      app_id, apps::IconEffects::kRoundCorners |
                  apps::IconEffects::kCrOsStandardBackground |
                  apps::IconEffects::kCrOsStandardMask);

  VerifyIcon(src_image_skia, dst);
}

TEST_F(WebAppIconFactoryTest, LoadExactSizeIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 48;
  const int kIconSize2 = 64;
  const int kIconSize3 = 96;
  const int kIconSize4 = 128;
  const int kIconSize5 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2, kIconSize3,
                                  kIconSize4, kIconSize5};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW,
                                    SK_ColorBLACK, SK_ColorRED, SK_ColorBLUE};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  gfx::ImageSkia src_image_skia =
      test_helper().GenerateWebAppIcon(app_id, IconPurpose::ANY, sizes_px,
                                       {{1.0, kIconSize2}, {2.0, kIconSize4}});

  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;

  icon_effect |= apps::IconEffects::kCrOsStandardIcon;

  gfx::ImageSkia dst = LoadIconFromWebApp(app_id, icon_effect);

  VerifyIcon(src_image_skia, dst);
}

TEST_F(WebAppIconFactoryTest, LoadIconFailed) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 48;
  const int kIconSize2 = 64;
  const int kIconSize3 = 96;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2, kIconSize3};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW,
                                    SK_ColorBLACK};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  gfx::ImageSkia src_image_skia;
  LoadDefaultIcon(src_image_skia);

  gfx::ImageSkia dst =
      LoadIconFromWebApp(app_id, apps::IconEffects::kRoundCorners |
                                     apps::IconEffects::kCrOsStandardIcon);

  VerifyIcon(src_image_skia, dst);
}

}  // namespace apps
