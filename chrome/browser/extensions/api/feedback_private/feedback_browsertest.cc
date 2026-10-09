// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/feedback_private.h"

using extensions::api::feedback_private::FeedbackFlow;

namespace extensions {

// TODO(http://b/230376167): Remove or migrate the tests to work with WebUI
// Feedback.
class FeedbackTest : public ExtensionBrowserTest {
 public:
  void SetUp() override {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    ExtensionBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnableUserMediaScreenCapturing);
  }

 protected:
  bool IsFeedbackAppAvailable() { return false; }

  void StartFeedbackUI(FeedbackFlow flow,
                       const std::string& extra_diagnostics,
                       bool from_assistant = false,
                       bool include_bluetooth_logs = false,
                       bool show_questionnaire = false) {}

  void VerifyFeedbackAppLaunch() {}
};

// TODO(crbug.com/40194697): disable tests.
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_ShowFeedback) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::kRegular, std::string());
  VerifyFeedbackAppLaunch();
}

// Disabled due to flake: https://crbug.com/40126107
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_GetTargetTabUrl) {
  const std::pair<std::string, std::string> test_cases[] = {
      {"https://www.google.com/", "https://www.google.com/"},
      {"chrome://version/", chrome::kChromeUIVersionURL},
      {chrome::kChromeUIBookmarksURL, chrome::kChromeUIBookmarksURL},
  };

  for (const auto& test_case : test_cases) {
    GURL expected_url = GURL(test_case.second);

    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), GURL(test_case.first)));

    // Sanity check that we always have one tab in the browser.
    ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

    ASSERT_EQ(expected_url, browser()
                                ->tab_strip_model()
                                ->GetWebContentsAt(0)
                                ->GetLastCommittedURL());

    ASSERT_EQ(expected_url, chrome::GetTargetTabUrl(browser(), 0));

    // Open a DevTools window.
    DevToolsWindow* devtools_window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);

    // Verify the expected url returned from GetTargetTabUrl against a
    // DevTools window.
    ASSERT_EQ(expected_url,
              chrome::GetTargetTabUrl(
                  DevToolsWindowTesting::Get(devtools_window)->browser(), 0));

    DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
  }
}

}  // namespace extensions
