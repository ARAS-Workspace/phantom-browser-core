// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_mac.h"

#include <AppKit/AppKit.h>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/hit_test.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#include "ui/events/test/cocoa_test_event_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_view.h"
#include "url/gurl.h"

class VerticalTabStripDoubleClickMacTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {};

// Test that double-clicking on the empty area of the vertical tab strip
// zooms the window according to the macOS system preference.
IN_PROC_BROWSER_TEST_F(VerticalTabStripDoubleClickMacTest,
                       DoubleClickOnEmptyAreaZoomsWindow) {
  VerticalTabStripRegionView* view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->vertical_tab_strip_region_view_for_testing();
  ASSERT_TRUE(view);
  ASSERT_TRUE(view->GetVisible());

  // Pick a point in the empty space below all tabs. The vertical center of
  // the bottom half of the region view should be well below the last tab.
  const gfx::Rect bounds = view->GetLocalBounds();
  gfx::Point caption_point = bounds.bottom_center();
  caption_point.Offset(0, -bounds.height() / 4);
  ASSERT_TRUE(view->IsPositionInWindowCaption(caption_point));

  // Convert to widget (window) coordinates.
  views::View::ConvertPointToWidget(view, &caption_point);

  // Override the system preference for the test and ensure it's restored.
  NSString* const kPrefKey = @"AppleActionOnDoubleClick";
  NSString* original_action =
      [[NSUserDefaults standardUserDefaults] stringForKey:kPrefKey];
  [[NSUserDefaults standardUserDefaults] setObject:@"Maximize" forKey:kPrefKey];
  // Should be restored at the end of the test, even if it fails.
  base::ScopedClosureRunner restore_pref(base::BindOnce(^{
    if (original_action) {
      [[NSUserDefaults standardUserDefaults] setObject:original_action
                                                forKey:kPrefKey];
    } else {
      [[NSUserDefaults standardUserDefaults] removeObjectForKey:kPrefKey];
    }
  }));

  NSWindow* ns_window =
      browser()->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  ASSERT_TRUE(ns_window);

  // Widget coordinates use top-left origin; NSWindow coordinates use
  // bottom-left origin. Since Chromium uses NSFullSizeContentViewWindowMask,
  // the content view fills the entire window frame.
  NSPoint ns_point =
      NSMakePoint(caption_point.x(),
                  NSHeight([ns_window.contentView frame]) - caption_point.y());

  EXPECT_FALSE([ns_window isZoomed]);

  // Send a double-click (mouseDown + mouseUp with clickCount=2) via
  // cocoa_test_event_utils.
  [ns_window sendEvent:cocoa_test_event_utils::MouseEventAtPointInWindow(
                           ns_point, NSEventTypeLeftMouseDown, ns_window, 2)];
  [ns_window sendEvent:cocoa_test_event_utils::MouseEventAtPointInWindow(
                           ns_point, NSEventTypeLeftMouseUp, ns_window, 2)];

  EXPECT_TRUE([ns_window isZoomed]);
}
