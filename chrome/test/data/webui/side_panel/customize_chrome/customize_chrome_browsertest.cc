// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class SidePanelCustomizeChromeTest : public WebUIMochaBrowserTest {
 protected:
  SidePanelCustomizeChromeTest() {
    set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  }
};

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, ButtonLabel) {
  RunTest("side_panel/customize_chrome/button_label_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, App) {
  RunTest("side_panel/customize_chrome/app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, Appearance) {
  RunTest("side_panel/customize_chrome/appearance_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, Footer) {
  RunTest("side_panel/customize_chrome/footer_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, HoverButton) {
  RunTest("side_panel/customize_chrome/hover_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, Toolbar) {
  RunTest("side_panel/customize_chrome/toolbar_test.js", "mocha.run()");
}
