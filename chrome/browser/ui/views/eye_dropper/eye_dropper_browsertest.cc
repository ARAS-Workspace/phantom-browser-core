// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "ui/display/display_switches.h"

// TODO(crbug.com/40269208): enable this test on all supported platforms.

class EyeDropperBrowserTest : public UiBrowserTest,
                              public ::testing::WithParamInterface<float> {
 public:
  EyeDropperBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::NumberToString(GetParam()));
  }

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
  }

  bool VerifyUi() override {
    return true;
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::BrowserDestroyedObserver().Wait();
  }

  void DismissUi() override { eye_dropper_.reset(); }

 private:
  std::unique_ptr<content::EyeDropper> eye_dropper_;
};

// Invokes the eye dropper.
// Flaky: https://crbug.com/40150152, https://crbug.com/402170536
IN_PROC_BROWSER_TEST_P(EyeDropperBrowserTest, DISABLED_InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         EyeDropperBrowserTest,
                         testing::Values(1.0, 1.5, 2.0));
