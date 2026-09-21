// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_creator.h"

#import <Cocoa/Cocoa.h>
#include <errno.h>
#include <stddef.h>
#include <sys/xattr.h>

#include <memory>
#include <optional>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/web_applications/os_integration/mac/apps_folder_support.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_auto_login_util.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#import "chrome/common/mac/app_mode_common.h"
#include "chrome/grit/theme_resources.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

namespace web_app {

namespace {

class WebAppShortcutCreatorMock : public WebAppShortcutCreator {
 public:
  WebAppShortcutCreatorMock(const base::FilePath& app_data_dir,
                            const ShortcutInfo* shortcut_info)
      : WebAppShortcutCreator(app_data_dir,
                              GetChromeAppsFolder(),
                              shortcut_info,
                              web_app::UseAdHocSigningForWebAppShims()) {}

  MOCK_CONST_METHOD0(GetAppBundlesByIdUnsorted, std::vector<base::FilePath>());
  MOCK_CONST_METHOD1(RevealAppShimInFinder, void(const base::FilePath&));

  WebAppShortcutCreatorMock(const WebAppShortcutCreatorMock&) = delete;
  WebAppShortcutCreatorMock& operator=(const WebAppShortcutCreatorMock&) =
      delete;
};

class WebAppShortcutCreatorSortingMock : public WebAppShortcutCreator {
 public:
  WebAppShortcutCreatorSortingMock(const base::FilePath& app_data_dir,
                                   const ShortcutInfo* shortcut_info)
      : WebAppShortcutCreator(app_data_dir,
                              GetChromeAppsFolder(),
                              shortcut_info,
                              web_app::UseAdHocSigningForWebAppShims()) {}

  MOCK_CONST_METHOD0(GetAppBundlesByIdUnsorted, std::vector<base::FilePath>());

  WebAppShortcutCreatorSortingMock(const WebAppShortcutCreatorSortingMock&) =
      delete;
  WebAppShortcutCreatorSortingMock& operator=(
      const WebAppShortcutCreatorSortingMock&) = delete;
};

class WebAppAutoLoginUtilMock : public WebAppAutoLoginUtil {
 public:
  WebAppAutoLoginUtilMock() = default;
  WebAppAutoLoginUtilMock(const WebAppAutoLoginUtilMock&) = delete;
  WebAppAutoLoginUtilMock& operator=(const WebAppAutoLoginUtilMock&) = delete;

  void AddToLoginItems(const base::FilePath& app_bundle_path,
                       bool hide_on_startup) override {
    EXPECT_TRUE(base::PathExists(app_bundle_path));
    EXPECT_FALSE(hide_on_startup);
    add_to_login_items_called_count_++;
  }

  void RemoveFromLoginItems(const base::FilePath& app_bundle_path) override {
    EXPECT_TRUE(base::PathExists(app_bundle_path));
    remove_from_login_items_called_count_++;
  }

  void ResetCounts() {
    add_to_login_items_called_count_ = 0;
    remove_from_login_items_called_count_ = 0;
  }

  int GetAddToLoginItemsCalledCount() const {
    return add_to_login_items_called_count_;
  }

  int GetRemoveFromLoginItemsCalledCount() const {
    return remove_from_login_items_called_count_;
  }

 private:
  int add_to_login_items_called_count_ = 0;
  int remove_from_login_items_called_count_ = 0;
};

std::unique_ptr<ShortcutInfo> GetShortcutInfo() {
  std::unique_ptr<ShortcutInfo> info(new ShortcutInfo);
  info->app_id = "appid";
  info->title = u"Shortcut Title";
  info->url = GURL("http://example.com/");
  info->profile_path = base::FilePath("user_data_dir").Append("Profile 1");
  info->profile_name = "profile name";
  info->version_for_display = "stable 1.0";
  info->is_multi_profile = true;
  return info;
}

class WebAppShortcutCreatorTest : public testing::Test {
 public:
  WebAppShortcutCreatorTest(const WebAppShortcutCreatorTest&) = delete;
  WebAppShortcutCreatorTest& operator=(const WebAppShortcutCreatorTest&) =
      delete;

 protected:
  WebAppShortcutCreatorTest() = default;

  void SetUp() override {
    override_registration_ =
        OsIntegrationTestOverrideImpl::OverrideForTesting();
    destination_dir_ =
        override_registration_->test_override().chrome_apps_folder();

    EXPECT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
    user_data_dir_ = temp_user_data_dir_.GetPath();
    // Recreate the directory structure as it would be created for the
    // ShortcutInfo created in the above GetShortcutInfo.
    app_data_dir_ = user_data_dir_.Append("Profile 1")
                        .Append("Web Applications")
                        .Append("_crx_extensionid");
    EXPECT_TRUE(base::CreateDirectory(app_data_dir_));

    // When using base::PathService::Override, it calls
    // base::MakeAbsoluteFilePath. On Mac this prepends "/private" to the path,
    // but points to the same directory in the file system.
    user_data_dir_override_.emplace(chrome::DIR_USER_DATA, user_data_dir_);
    user_data_dir_ = base::MakeAbsoluteFilePath(user_data_dir_);
    app_data_dir_ = base::MakeAbsoluteFilePath(app_data_dir_);

    info_ = GetShortcutInfo();
    fallback_shim_base_name_ = base::FilePath(
        info_->profile_path.BaseName().value() + " " + info_->app_id + ".app");

    shim_base_name_ = base::FilePath(base::UTF16ToUTF8(info_->title) + ".app");
    shim_path_ = destination_dir_.Append(shim_base_name_);

    auto_login_util_mock_ = std::make_unique<WebAppAutoLoginUtilMock>();
    WebAppAutoLoginUtil::SetInstanceForTesting(auto_login_util_mock_.get());

    // Make sure that the tests in this class will actually try to set the
    // localized app dir name.
    ResetHaveLocalizedAppDirNameForTesting();
  }

  void TearDown() override {
    WebAppAutoLoginUtil::SetInstanceForTesting(nullptr);
    override_registration_.reset();
    testing::Test::TearDown();
  }

  // Needed by DCHECK_CURRENTLY_ON in ShortcutInfo destructor.
  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir temp_user_data_dir_;
  base::FilePath app_data_dir_;
  base::FilePath destination_dir_;
  base::FilePath user_data_dir_;
  std::optional<base::ScopedPathOverride> user_data_dir_override_;

  std::unique_ptr<WebAppAutoLoginUtilMock> auto_login_util_mock_;
  std::unique_ptr<ShortcutInfo> info_;
  base::FilePath fallback_shim_base_name_;
  base::FilePath shim_base_name_;
  base::FilePath shim_path_;

  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

}  // namespace

TEST_F(WebAppShortcutCreatorTest, NormalizeTitle) {
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());

  info_->title = u"../../Evil/";
  EXPECT_EQ(destination_dir_.Append(":..:Evil:.app"),
            shortcut_creator.GetApplicationsShortcutPath(false));

  info_->title = u"....";
  EXPECT_EQ(destination_dir_.Append(fallback_shim_base_name_),
            shortcut_creator.GetApplicationsShortcutPath(false));
}

TEST_F(WebAppShortcutCreatorTest, CreateFailure) {
  ASSERT_TRUE(override_registration_->test_override().DeleteChromeAppsDir());

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(app_data_dir_,
                                                       info_.get());
  EXPECT_FALSE(shortcut_creator.CreateShortcuts(SHORTCUT_CREATION_AUTOMATED,
                                                ShortcutLocations()));
}

TEST_F(WebAppShortcutCreatorTest, UpdateIcon) {
  gfx::Image product_logo_16 =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_PRODUCT_LOGO_16);
  gfx::Image product_logo_32 =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_PRODUCT_LOGO_32);

  WebAppShortcutCreatorMock shortcut_creator(app_data_dir_, info_.get());
  base::FilePath icon_path =
      shim_path_.Append("Contents").Append("Resources").Append("app.icns");

  // regular favicon should be used if no maskable favicons exist
  info_->favicon.Add(product_logo_32);
  ASSERT_TRUE(shortcut_creator.UpdateIcon(shim_path_));
  NSImage* image = [[NSImage alloc]
      initWithContentsOfFile:base::apple::FilePathToNSString(icon_path)];
  EXPECT_TRUE(image);
  EXPECT_EQ(product_logo_32.Width(), image.size.width);
  EXPECT_EQ(product_logo_32.Height(), image.size.height);

  // maskable favicon should be used if present
  info_->favicon_maskable.Add(product_logo_16);
  ASSERT_TRUE(shortcut_creator.UpdateIcon(shim_path_));
  image = [[NSImage alloc]
      initWithContentsOfFile:base::apple::FilePathToNSString(icon_path)];
  EXPECT_TRUE(image);
  EXPECT_EQ(product_logo_16.Width(), image.size.width);
  EXPECT_EQ(product_logo_16.Height(), image.size.height);
}

TEST_F(WebAppShortcutCreatorTest, SortAppBundles) {
  base::FilePath app_dir("/home/apps");
  NiceMock<WebAppShortcutCreatorSortingMock> shortcut_creator(app_dir,
                                                              info_.get());
  base::FilePath a = shortcut_creator.GetApplicationsShortcutPath(false);
  base::FilePath b = GetChromeAppsFolder().Append("a");
  base::FilePath c = GetChromeAppsFolder().Append("z");
  base::FilePath d("/a/b/c");
  base::FilePath e("/z/y/w");
  std::vector<base::FilePath> unsorted = {e, c, a, d, b};
  std::vector<base::FilePath> sorted = {a, b, c, d, e};

  EXPECT_CALL(shortcut_creator, GetAppBundlesByIdUnsorted())
      .WillOnce(Return(unsorted));
  std::vector<base::FilePath> result = shortcut_creator.GetAppBundlesById();
  EXPECT_EQ(result, sorted);
}

}  // namespace web_app
