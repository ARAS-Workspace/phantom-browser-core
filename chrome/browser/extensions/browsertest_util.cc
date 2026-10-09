// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browsertest_util.h"

#include <memory>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions::browsertest_util {

content::WebContents* AddTab(BrowserWindowInterface* browser, const GURL& url) {
  int starting_tab_count = browser->GetTabStripModel()->count();
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  int tab_count = browser->GetTabStripModel()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);
  return browser->GetTabStripModel()->GetActiveWebContents();
}

size_t GetWindowControllerCountInProfile(Profile* profile) {
  size_t count = 0;
  for (WindowController* window : *WindowControllerList::GetInstance()) {
    if (window->profile() == profile) {
      count++;
    }
  }
  return count;
}

}  // namespace extensions::browsertest_util
