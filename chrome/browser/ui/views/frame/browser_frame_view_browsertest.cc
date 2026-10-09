// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/theme_change_waiter.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/mock_os_settings_provider.h"

class BrowserFrameViewBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  BrowserFrameViewBrowserTest() = default;

  BrowserFrameViewBrowserTest(const BrowserFrameViewBrowserTest&) = delete;
  BrowserFrameViewBrowserTest& operator=(const BrowserFrameViewBrowserTest&) =
      delete;

  ~BrowserFrameViewBrowserTest() override = default;

  void SetUp() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    extensions::ExtensionBrowserTest::SetUp();
  }
};

// Tests the frame color for a normal browser window.
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest, BrowserFrameColorThemed) {
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  const BrowserFrameView* frame_view =
      browser_view->browser_widget()->GetFrameView();
  const ui::ColorProvider* color_provider = frame_view->GetColorProvider();
  const SkColor expected_active_color =
      color_provider->GetColor(ui::kColorFrameActive);
  const SkColor expected_inactive_color =
      color_provider->GetColor(ui::kColorFrameInactive);

  EXPECT_EQ(expected_active_color,
            frame_view->GetFrameColor(BrowserFrameActiveState::kActive));
  EXPECT_EQ(expected_inactive_color,
            frame_view->GetFrameColor(BrowserFrameActiveState::kInactive));
}

// Verifies that the incognito window frame is always the right color.
IN_PROC_BROWSER_TEST_F(BrowserFrameViewBrowserTest, IncognitoIsCorrectColor) {
  // Set the color that's expected to be ignored.
  ui::MockOsSettingsProvider os_settings_provider;
  os_settings_provider.SetAccentColor(gfx::kGoogleBlue400);

  Browser* incognito_browser = CreateIncognitoBrowser(browser()->GetProfile());

  BrowserView* view = BrowserView::GetBrowserViewForBrowser(incognito_browser);
  BrowserWidget* widget = view->browser_widget();
  BrowserFrameView* frame_view = widget->GetFrameView();

  color_utils::HSL frame_color_hsl;
  SkColorToHSL(frame_view->GetFrameColor(BrowserFrameActiveState::kActive),
               &frame_color_hsl);
  // Ensure that the frame color is very dark in Incognito.
  EXPECT_LT(frame_color_hsl.l, 0.2);

  incognito_browser->GetWindow()->Close();
}

using BrowserFrameViewPopupTest = InProcessBrowserTest;

// TODO(crbug.com/41478509): Flaky on Linux TSAN and ASAN.
#if BUILDFLAG(IS_LINUX) && \
    (defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER))
#define MAYBE_HitTestPopupTopChrome DISABLED_HitTestPopupTopChrome
#else
#define MAYBE_HitTestPopupTopChrome HitTestPopupTopChrome
#endif
IN_PROC_BROWSER_TEST_F(BrowserFrameViewPopupTest, MAYBE_HitTestPopupTopChrome) {
  Browser* popup_browser = CreateBrowserForPopup(browser()->GetProfile());
  BrowserView* popup_browser_view =
      BrowserView::GetBrowserViewForBrowser(popup_browser);
  BrowserFrameView* frame_view =
      popup_browser_view->browser_widget()->GetFrameView();

  constexpr gfx::Rect kLeftOfFrame(-1, 4, 1, 1);
  EXPECT_FALSE(frame_view->HitTestRect(kLeftOfFrame));

  constexpr gfx::Rect kAboveFrame(4, -1, 1, 1);
  EXPECT_FALSE(frame_view->HitTestRect(kAboveFrame));

  const int top_inset = frame_view->GetTopInset(false);
  const gfx::Rect in_browser_view(4, top_inset, 1, 1);
  EXPECT_TRUE(frame_view->HitTestRect(in_browser_view));
}

using BrowserFrameViewTabbedTest = InProcessBrowserTest;

// TODO(crbug.com/40101869): Flaky on Linux TSAN.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_HitTestTabstrip DISABLED_HitTestTabstrip
#else
#define MAYBE_HitTestTabstrip HitTestTabstrip
#endif

IN_PROC_BROWSER_TEST_F(BrowserFrameViewTabbedTest, MAYBE_HitTestTabstrip) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  BrowserFrameView* frame_view = browser_view->browser_widget()->GetFrameView();

  const gfx::Rect frame_bounds = frame_view->bounds();

  gfx::RectF tabstrip_bounds_in_frame_coords(
      browser_view->horizontal_tab_strip_for_testing()->GetLocalBounds());
  views::View::ConvertRectToTarget(
      browser_view->horizontal_tab_strip_for_testing(), frame_view,
      &tabstrip_bounds_in_frame_coords);
  const gfx::Rect tabstrip_bounds =
      gfx::ToEnclosingRect(tabstrip_bounds_in_frame_coords);
  EXPECT_FALSE(tabstrip_bounds.IsEmpty());

  // Completely outside the frame's bounds.
  EXPECT_FALSE(frame_view->HitTestRect(
      gfx::Rect(frame_bounds.x() - 1, frame_bounds.y() + 1, 1, 1)));
  EXPECT_FALSE(frame_view->HitTestRect(
      gfx::Rect(frame_bounds.x() + 1, frame_bounds.y() - 1, 1, 1)));

  // Hits client portions of the tabstrip (near the bottom left corner of the
  // first tab).
  EXPECT_TRUE(frame_view->HitTestRect(gfx::Rect(
      tabstrip_bounds.x() + 10, tabstrip_bounds.bottom() - 10, 1, 1)));
  EXPECT_TRUE(browser_view->HitTestRect(gfx::Rect(
      tabstrip_bounds.x() + 10, tabstrip_bounds.bottom() - 10, 1, 1)));

  // Hits non-client portions of the tab strip (the top left corner of the
  // first tab).
  EXPECT_TRUE(frame_view->HitTestRect(
      gfx::Rect(tabstrip_bounds.x(), tabstrip_bounds.y(), 1, 1)));

  // Hits tab strip and the browser-client area.
  EXPECT_TRUE(frame_view->HitTestRect(gfx::Rect(
      tabstrip_bounds.x() + 1,
      tabstrip_bounds.bottom() -
          GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap) - 1,
      100, 100)));
}
