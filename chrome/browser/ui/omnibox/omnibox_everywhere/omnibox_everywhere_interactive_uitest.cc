// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/view_type_utils.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace omnibox_everywhere {

class OmniboxEverywhereBrowserTest : public InteractiveBrowserTest {
 public:
  OmniboxEverywhereBrowserTest() {
    feature_list_.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
  }
  ~OmniboxEverywhereBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, ShowAndCloseWidget) {
  OmniboxEverywhereUIManager ui_manager;

  EXPECT_FALSE(ui_manager.widget());

  // Show the widget.
  ui_manager.ShowForProfile(browser()->GetProfile(),
                            browser()->GetWindow()->GetNativeWindow());

  views::Widget* widget = ui_manager.widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Check the widget delegate and its properties.
  views::WidgetDelegate* delegate = widget->widget_delegate();
  ASSERT_TRUE(delegate);
  EXPECT_TRUE(delegate->CanActivate());
  EXPECT_FALSE(delegate->CanMaximize());
  EXPECT_FALSE(delegate->CanMinimize());
  EXPECT_FALSE(delegate->CanResize());

  // Close (hide) the widget.
  ui_manager.Close();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_TRUE(ui_manager.widget());

  // Shutdown destroys the widget.
  ui_manager.Shutdown();
  EXPECT_FALSE(ui_manager.widget());
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, FocusAndActivationState) {
  OmniboxEverywhereUIManager ui_manager;

  ui_manager.ShowForProfile(browser()->GetProfile(),
                            browser()->GetWindow()->GetNativeWindow());
  views::Widget* widget = ui_manager.widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  views::test::WaitForWidgetActive(widget, true);
  EXPECT_TRUE(widget->IsActive());

  ui_manager.Close();
  EXPECT_FALSE(widget->IsVisible());
  ui_manager.Shutdown();
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest, VoicePermissionState) {
  OmniboxEverywhereUIManager ui_manager;
  ui_manager.ShowForProfile(browser()->GetProfile(),
                            browser()->GetWindow()->GetNativeWindow());

  ASSERT_TRUE(ui_manager.contents_wrapper_for_testing());
  content::WebContents* contents =
      ui_manager.contents_wrapper_for_testing()->web_contents();
  ASSERT_TRUE(contents);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  EXPECT_EQ(extensions::GetViewType(contents),
            extensions::mojom::ViewType::kComponent);
#endif
  EXPECT_TRUE(permissions::PermissionRequestManager::FromWebContents(contents));

  ui_manager.Close();
  EXPECT_FALSE(ui_manager.widget()->IsVisible());
  ui_manager.Shutdown();
  EXPECT_FALSE(ui_manager.widget());
}

#define MAYBE_StatusIconLifecycle StatusIconLifecycle
IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       MAYBE_StatusIconLifecycle) {
  StatusTray* status_tray = g_browser_process->status_tray();
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);

  if (!status_tray) {
    GTEST_SKIP() << "StatusTray is not supported on this platform.";
  }

  // Initially background mode pref is false, status icon should not exist.
  EXPECT_FALSE(status_tray->HasStatusIconOfTypeForTesting(
      StatusTray::OMNIBOX_EVERYWHERE_ICON));

  // Enable background mode pref.
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
  EXPECT_TRUE(status_tray->HasStatusIconOfTypeForTesting(
      StatusTray::OMNIBOX_EVERYWHERE_ICON));

  // Disable background mode pref.
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, false);
  EXPECT_FALSE(status_tray->HasStatusIconOfTypeForTesting(
      StatusTray::OMNIBOX_EVERYWHERE_ICON));
}

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereBrowserTest,
                       ShutdownWithBackgroundModeEnabled) {
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);

  set_exit_when_last_browser_closes(false);

  // Enable background mode pref.
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);

  // Close the browser window synchronously first. Since background mode is
  // enabled, this should not shut down the browser process.
  CloseBrowserSynchronously(browser());

  // Post a task to quit the browser once the main message loop starts.
  // This ensures that the quit flow executes while the main RunLoop is running,
  // preventing CHECK failures in BrowserProcessImpl::StartTearDown.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&chrome::CloseAllBrowsersAndQuit));
}

}  // namespace omnibox_everywhere
