// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/display/types/display_constants.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

class DevToolsManagerDelegateTest : public InProcessBrowserTest {
 public:
  void SendCommand(const std::string& state) {
    auto window_bounds =
        protocol::Browser::Bounds::Create().SetWindowState(state).Build();
    BrowserHandler handler(nullptr, "");
    handler.SetWindowBounds(browser()->GetSessionID().id(),
                            std::move(window_bounds));
  }

  void UpdateBounds() {
    auto window_bounds = protocol::Browser::Bounds::Create()
                             .SetWindowState("normal")
                             .SetLeft(200)
                             .SetHeight(400)
                             .Build();
    BrowserHandler handler(nullptr, "");
    handler.SetWindowBounds(browser()->GetSessionID().id(),
                            std::move(window_bounds));
  }

  void CheckIsMaximized(bool maximized) {
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&ui::BaseWindow::IsMaximized,
                            base::Unretained(browser()->GetWindow())),
        maximized, base::Seconds(1))
        .Wait();
    EXPECT_EQ(maximized, browser()->GetWindow()->IsMaximized());
  }

  void CheckIsMinimized(bool minimized) {
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&ui::BaseWindow::IsMinimized,
                            base::Unretained(browser()->GetWindow())),
        minimized, base::Seconds(1))
        .Wait();
    EXPECT_EQ(minimized, browser()->GetWindow()->IsMinimized());
  }

  void CheckIsFullscreen(bool fullscreen) {
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&ui::BaseWindow::IsFullscreen,
                            base::Unretained(browser()->GetWindow())),
        fullscreen, base::Seconds(1))
        .Wait();
    EXPECT_EQ(fullscreen, browser()->GetWindow()->IsFullscreen());
  }

  bool IsWindowBoundsEqual(gfx::Rect expected) {
    return browser()->GetWindow()->GetBounds() == expected;
  }

  void CheckWindowBounds(gfx::Rect expected) {
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&DevToolsManagerDelegateTest::IsWindowBoundsEqual,
                            base::Unretained(this), expected),
        true, base::Seconds(1))
        .Wait();
    EXPECT_EQ(expected, browser()->GetWindow()->GetBounds());
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, NormalWindowChangeBounds) {
  browser()->GetWindow()->SetBounds(gfx::Rect(100, 100, 600, 600));
  CheckWindowBounds(gfx::Rect(100, 100, 600, 600));
  UpdateBounds();
  CheckWindowBounds(gfx::Rect(200, 100, 600, 400));
}

#if BUILDFLAG(IS_MAC)
// MacViews does not yet implement maximized windows: https://crbug.com/41385204
#define MAYBE_NormalToMaximizedWindow DISABLED_NormalToMaximizedWindow
#else
#define MAYBE_NormalToMaximizedWindow NormalToMaximizedWindow
#endif
IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest,
                       MAYBE_NormalToMaximizedWindow) {
  CheckIsMaximized(false);
  SendCommand("maximized");
  CheckIsMaximized(true);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, NormalToMinimizedWindow) {
  CheckIsMinimized(false);
  SendCommand("minimized");
  CheckIsMinimized(true);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, NormalToFullscreenWindow) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen faker;
#endif
  CheckIsFullscreen(false);
  SendCommand("fullscreen");
  CheckIsFullscreen(true);
}

#if BUILDFLAG(IS_MAC)
// MacViews does not yet implement maximized windows: https://crbug.com/41385204
#define MAYBE_MaximizedToMinimizedWindow DISABLED_MaximizedToMinimizedWindow
#else
#define MAYBE_MaximizedToMinimizedWindow MaximizedToMinimizedWindow
#endif
IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest,
                       MAYBE_MaximizedToMinimizedWindow) {
  browser()->GetWindow()->Maximize();
  CheckIsMaximized(true);

  CheckIsMinimized(false);
  SendCommand("minimized");
  CheckIsMinimized(true);
}

#if BUILDFLAG(IS_MAC)
// MacViews does not yet implement maximized windows: https://crbug.com/41385204
#define MAYBE_MaximizedToFullscreenWindow DISABLED_MaximizedToFullscreenWindow
#else
#define MAYBE_MaximizedToFullscreenWindow MaximizedToFullscreenWindow
#endif
IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest,
                       MAYBE_MaximizedToFullscreenWindow) {
  browser()->GetWindow()->Maximize();
  CheckIsMaximized(true);

  CheckIsFullscreen(false);
  SendCommand("fullscreen");
  CheckIsFullscreen(true);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, ShowMinimizedWindow) {
  browser()->GetWindow()->Minimize();
  CheckIsMinimized(true);
  SendCommand("normal");
  CheckIsMinimized(false);
}

#if BUILDFLAG(IS_MAC)
// MacViews does not yet implement maximized windows: https://crbug.com/41385204
#define MAYBE_RestoreMaximizedWindow DISABLED_RestoreMaximizedWindow
#else
#define MAYBE_RestoreMaximizedWindow RestoreMaximizedWindow
#endif
IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest,
                       MAYBE_RestoreMaximizedWindow) {
  browser()->GetWindow()->Maximize();
  CheckIsMaximized(true);
  SendCommand("normal");
  CheckIsMaximized(false);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, ExitFullscreenWindow) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen faker;
#endif
  BrowserWindow::FromBrowser(browser())
      ->GetExclusiveAccessContext()
      ->EnterFullscreen(url::Origin(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
                        FullscreenTabParams());
  CheckIsFullscreen(true);
  SendCommand("normal");
  CheckIsFullscreen(false);
}
