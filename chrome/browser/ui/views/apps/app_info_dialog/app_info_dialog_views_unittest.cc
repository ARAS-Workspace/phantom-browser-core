// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/app_constants/constants.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestOtherExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

}  // namespace

class AppInfoDialogViewsTest : public ChromeViewsTestBase,
                               public views::WidgetObserver {
 public:
  AppInfoDialogViewsTest() = default;

  AppInfoDialogViewsTest(const AppInfoDialogViewsTest&) = delete;
  AppInfoDialogViewsTest& operator=(const AppInfoDialogViewsTest&) = delete;

  void SetUp() override {

    ChromeViewsTestBase::SetUp();

    extension_ = extension_environment_.MakePackagedApp(kTestExtensionId, true);
    chrome_app_ = extension_environment_.MakePackagedApp(
        app_constants::kChromeAppId, true);
  }

  void TearDown() override {
    CloseAppInfo();
    extension_ = nullptr;
    chrome_app_ = nullptr;

    extension_environment_.DeleteProfile();

    ChromeViewsTestBase::TearDown();

  }

 protected:
  void ShowAppInfo(const std::string& app_id) {
    ShowAppInfoForProfile(app_id, extension_environment_.profile());
  }

  void ShowAppInfoForProfile(const std::string& app_id, Profile* profile) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)
            ->enabled_extensions()
            .GetByID(app_id);
    DCHECK(extension);

    DCHECK(!widget_);
    widget_ = views::DialogDelegate::CreateDialogWidget(
        new views::DialogDelegateView(), GetContext(), gfx::NativeView());
    widget_->AddObserver(this);
    dialog_ = widget_->GetContentsView()->AddChildView(
        std::make_unique<AppInfoDialog>(profile, extension));
    widget_->Show();
  }

  void CloseAppInfo() {
    if (widget_) {
      widget_->CloseNow();
    }
    base::RunLoop().RunUntilIdle();
    DCHECK(!widget_);
  }

  // Overridden from views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
  }

  void UninstallApp(const std::string& app_id) {
    extensions::ExtensionRegistrar::Get(extension_environment_.profile())
        ->UninstallExtension(
            app_id, extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING,
            nullptr);
  }

  raw_ptr<views::Widget> widget_ = nullptr;
  raw_ptr<AppInfoDialog, AcrossTasksDanglingUntriaged> dialog_ =
      nullptr;  // Owned by |widget_|'s views hierarchy.
  scoped_refptr<const extensions::Extension> extension_;
  scoped_refptr<const extensions::Extension> chrome_app_;
  extensions::TestExtensionEnvironment extension_environment_{
      extensions::TestExtensionEnvironment::Type::
          kInheritExistingTaskEnvironment,
      extensions::TestExtensionEnvironment::ProfileCreationType::kCreate,
  };
};

// Tests that the dialog closes when the current app is uninstalled.
TEST_F(AppInfoDialogViewsTest, UninstallingAppClosesDialog) {
  ShowAppInfo(kTestExtensionId);
  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());
  UninstallApp(kTestExtensionId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(widget_);
}

// Tests that the dialog does not close when a different app is uninstalled.
TEST_F(AppInfoDialogViewsTest, UninstallingOtherAppDoesNotCloseDialog) {
  ShowAppInfo(kTestExtensionId);
  extension_environment_.MakePackagedApp(kTestOtherExtensionId, true);
  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());
  UninstallApp(kTestOtherExtensionId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(widget_);
}

// Exclude the test from ChromeOS because profile destruction does not happen
// on ChromeOS in production.
//
// Tests that the dialog closes when the current profile is destroyed.
TEST_F(AppInfoDialogViewsTest, DestroyedProfileClosesDialog) {
  ShowAppInfo(kTestExtensionId);
  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());

  // Delete the profile.
  extension_environment_.DeleteProfile();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(widget_);
}

// Tests that the dialog does not close when a different profile is destroyed.
TEST_F(AppInfoDialogViewsTest, DestroyedOtherProfileDoesNotCloseDialog) {
  ShowAppInfo(kTestExtensionId);
  std::unique_ptr<TestingProfile> other_profile(new TestingProfile);
  extension_environment_.CreateExtensionServiceForProfile(other_profile.get());

  scoped_refptr<const extensions::Extension> other_app =
      extension_environment_.MakePackagedApp(kTestOtherExtensionId, false);
  extensions::ExtensionRegistrar::Get(other_profile.get())
      ->AddExtension(other_app.get());

  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());
  other_profile.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(widget_);
}
