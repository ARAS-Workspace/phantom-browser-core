// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserTabId2);
}  // namespace

class HelpBubbleHandlerInteractiveUiTest : public InteractiveBrowserTest {
 public:
  HelpBubbleHandlerInteractiveUiTest() = default;
  ~HelpBubbleHandlerInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleHandlerInteractiveUiTest,
                       ElementBecomesVisibleOnPageLoad) {
  RunTestSequence(
      InstrumentTab(kBrowserTabId),
      NavigateWebContents(kBrowserTabId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      InAnyContext(WaitForShow(kWebUIIPHDemoElementIdentifier)));
}

IN_PROC_BROWSER_TEST_F(HelpBubbleHandlerInteractiveUiTest,
                       ElementBecomesHiddenOnTabBackgrounded) {
  RunTestSequence(
      InstrumentTab(kBrowserTabId),
      NavigateWebContents(kBrowserTabId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      InAnyContext(WaitForShow(kWebUIIPHDemoElementIdentifier)),
      // This will add the new tab in the foreground.
      AddInstrumentedTab(kBrowserTabId2, GURL(chrome::kChromeUIBookmarksURL)),
      InAnyContext(WaitForHide(kWebUIIPHDemoElementIdentifier)));
}
