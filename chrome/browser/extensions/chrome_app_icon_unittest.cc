// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_app_icon.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_app_icon_delegate.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/extensions/chrome_app_icon_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/app_icon_loader/app_icon_loader_delegate.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/constants.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace extensions {

namespace {

constexpr char kTestAppId[] = "emfkafnhnpcmabnnkckkchdilgeoekbo";

// Receives icon image updates from ChromeAppIcon.
class TestAppIcon : public ChromeAppIconDelegate {
 public:
  TestAppIcon(content::BrowserContext* context,
              const std::string& app_id,
              int size,
              const ChromeAppIconService::ResizeFunction& resize_function) {
    app_icon_ = ChromeAppIconService::Get(context)->CreateIcon(
        this, app_id, size, resize_function);
    DCHECK(app_icon_);
  }

  TestAppIcon(content::BrowserContext* context,
              const std::string& app_id,
              int size) {
    app_icon_ =
        ChromeAppIconService::Get(context)->CreateIcon(this, app_id, size);
    DCHECK(app_icon_);
  }

  TestAppIcon(const TestAppIcon&) = delete;
  TestAppIcon& operator=(const TestAppIcon&) = delete;

  ~TestAppIcon() override = default;

  void Reset() { app_icon_.reset(); }

  int GetIconUpdateCountAndReset() {
    int icon_update_count = icon_update_count_;
    icon_update_count_ = 0;
    return icon_update_count;
  }

  size_t icon_update_count() const { return icon_update_count_; }
  ChromeAppIcon* app_icon() { return app_icon_.get(); }
  const gfx::ImageSkia& image_skia() const { return app_icon_->image_skia(); }

  void WaitForIconUpdates() {
    base::RunLoop run_loop;
    icon_update_count_expected_ =
        icon_update_count_ + image_skia().image_reps().size();
    icon_updated_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnIconUpdated(ChromeAppIcon* icon) override {
    ++icon_update_count_;
    if (icon_update_count_ == icon_update_count_expected_ &&
        icon_updated_callback_) {
      std::move(icon_updated_callback_).Run();
    }
  }

  std::unique_ptr<ChromeAppIcon> app_icon_;

  size_t icon_update_count_ = 0;
  size_t icon_update_count_expected_ = 0;

  base::OnceClosure icon_updated_callback_;
};

// Receives icon image updates from ChromeAppIconLoader.
class TestAppIconLoader : public AppIconLoaderDelegate {
 public:
  TestAppIconLoader() = default;

  TestAppIconLoader(const TestAppIconLoader&) = delete;
  TestAppIconLoader& operator=(const TestAppIconLoader&) = delete;

  ~TestAppIconLoader() override = default;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(
      const std::string& app_id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) override {
    image_skia_ = image;
  }

  gfx::ImageSkia& icon() { return image_skia_; }
  const gfx::ImageSkia& icon() const { return image_skia_; }

 private:
  gfx::ImageSkia image_skia_;
};

// Returns true if provided |image| consists from only empty pixels.
bool IsBlankImage(const gfx::ImageSkia& image) {
  if (!image.width() || !image.height())
    return false;

  const SkBitmap* bitmap = image.bitmap();
  DCHECK(bitmap);
  DCHECK_EQ(bitmap->width(), image.width());
  DCHECK_EQ(bitmap->height(), image.height());

  for (int x = 0; x < bitmap->width(); ++x) {
    for (int y = 0; y < bitmap->height(); ++y) {
      if (*bitmap->getAddr32(x, y))
        return false;
    }
  }
  return true;
}

// Returns true if provided |image| is grayscale.
bool IsGrayscaleImage(const gfx::ImageSkia& image) {
  const SkBitmap* bitmap = image.bitmap();
  DCHECK(bitmap);
  for (int x = 0; x < bitmap->width(); ++x) {
    for (int y = 0; y < bitmap->height(); ++y) {
      const unsigned int pixel = *bitmap->getAddr32(x, y);
      if ((pixel & 0xff) != ((pixel >> 8) & 0xff) ||
          (pixel & 0xff) != ((pixel >> 16) & 0xff)) {
        return false;
      }
    }
  }
  return true;
}

// Returns true if provided |image1| and |image2| are equal.
bool AreEqual(const gfx::ImageSkia& image1, const gfx::ImageSkia& image2) {
  return gfx::test::AreImagesEqual(gfx::Image(image1), gfx::Image(image2));
}

}  // namespace

class ChromeAppIconTest : public ExtensionServiceTestBase {
 public:
  ChromeAppIconTest() = default;

  ChromeAppIconTest(const ChromeAppIconTest&) = delete;
  ChromeAppIconTest& operator=(const ChromeAppIconTest&) = delete;

  ~ChromeAppIconTest() override = default;

  // ExtensionServiceTestBase:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    ExtensionServiceInitParams params;
    ASSERT_TRUE(params.ConfigureByTestDataDirectory(
        data_dir().AppendASCII("app_list")));
    InitializeExtensionService(std::move(params));
    service()->Init();
  }
};

TEST_F(ChromeAppIconTest, IconLifeCycle) {
  TestAppIcon reference_icon(profile(), kTestAppId,
                             extension_misc::EXTENSION_ICON_MEDIUM);
  EXPECT_EQ(1U, reference_icon.icon_update_count());
  // By default no representation in image.
  EXPECT_FALSE(reference_icon.image_skia().HasRepresentation(1.0f));

  // Default blank image must be provided without an update.
  EXPECT_FALSE(reference_icon.image_skia().GetRepresentation(1.0f).is_null());
  EXPECT_EQ(1U, reference_icon.icon_update_count());
  EXPECT_TRUE(reference_icon.image_skia().HasRepresentation(1.0f));
  EXPECT_EQ(extension_misc::EXTENSION_ICON_MEDIUM,
            reference_icon.image_skia().width());
  EXPECT_EQ(extension_misc::EXTENSION_ICON_MEDIUM,
            reference_icon.image_skia().height());
  EXPECT_TRUE(IsBlankImage(reference_icon.image_skia()));

  // Wait until real image is loaded.
  reference_icon.WaitForIconUpdates();
  EXPECT_EQ(2U, reference_icon.icon_update_count());
  EXPECT_FALSE(IsBlankImage(reference_icon.image_skia()));
  EXPECT_FALSE(IsGrayscaleImage(reference_icon.image_skia()));

  const gfx::ImageSkia image_before_disable = reference_icon.image_skia();
  // Disable extension. This should update icon and provide grayed image inline.
  // Note update might be sent twice in CrOS.
  registrar()->DisableExtension(kTestAppId,
                                {disable_reason::DISABLE_CORRUPTED});
  const size_t update_count_after_disable = reference_icon.icon_update_count();
  EXPECT_NE(2U, update_count_after_disable);
  EXPECT_FALSE(IsBlankImage(reference_icon.image_skia()));
  EXPECT_TRUE(IsGrayscaleImage(reference_icon.image_skia()));

  // Reenable extension. It should match previous enabled image
  registrar()->EnableExtension(kTestAppId);
  EXPECT_NE(update_count_after_disable, reference_icon.icon_update_count());
  EXPECT_TRUE(AreEqual(reference_icon.image_skia(), image_before_disable));
}

// Validates that icon release is safe.
TEST_F(ChromeAppIconTest, IconRelease) {
  TestAppIcon test_icon1(profile(), kTestAppId,
                         extension_misc::EXTENSION_ICON_MEDIUM);
  TestAppIcon test_icon2(profile(), kTestAppId,
                         extension_misc::EXTENSION_ICON_MEDIUM);
  EXPECT_FALSE(test_icon1.image_skia().GetRepresentation(1.0f).is_null());
  EXPECT_FALSE(test_icon2.image_skia().GetRepresentation(1.0f).is_null());

  // Reset before service is stopped.
  test_icon1.Reset();

  // Reset after service is stopped.
  DeleteProfile();

  test_icon2.Reset();
}

}  // namespace extensions
