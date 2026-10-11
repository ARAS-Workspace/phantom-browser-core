// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDownloadsPageTabId);

const char kClickFn[] = "el => el.click()";

using ::testing::Eq;

class DownloadsPageInteractiveUitest
    : public InteractiveBrowserTestMixin<DownloadTestBase> {
 public:
  DownloadsPageInteractiveUitest() = default;
  ~DownloadsPageInteractiveUitest() override = default;

  void SetUp() override {
    // This is necessary so that opening the downloads page via app menu will
    // reliably open in the same tab. (It only opens in the same tab if the
    // current tab is about:blank.)
    set_open_about_blank_on_browser_launch(true);
    InteractiveBrowserTestMixin<DownloadTestBase>::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTestMixin<DownloadTestBase>::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Opens chrome://downloads via the app menu. All tests must begin with this
  // step so that the identifier kDownloadsPageTabId is bound properly.
  auto OpenDownloadsPage() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kReadyEvent);
    const DeepQuery kPathToDocumentHtml{
        "html",
    };
    const char kNotLoadingFn[] = "el => el.getAttribute('loading') === null";
    // Waits for the page to finish loading. (This attribute is applied manually
    // by the WebUI and has a TODO to be converted to a class instead, so this
    // may be fragile.)
    StateChange html_not_loading;
    html_not_loading.type = StateChange::Type::kExistsAndConditionTrue;
    html_not_loading.where = kPathToDocumentHtml;
    html_not_loading.test_function = kNotLoadingFn;
    html_not_loading.event = kReadyEvent;
    return Steps(InstrumentTab(kDownloadsPageTabId),
                 PressButton(kToolbarAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
                 WaitForWebContentsNavigation(
                     kDownloadsPageTabId, GURL(chrome::kChromeUIDownloadsURL)),
                 WaitForStateChange(kDownloadsPageTabId, html_not_loading));
  }

  // Path to an element in the topmost download item.
  DeepQuery PathToTopmostItemElement(const std::string& element_selector) {
    return DeepQuery{
        "downloads-manager",
        "downloads-item",
        element_selector,
    };
  }

  // Checks that an element is visible (or not visible) on the page.
  auto CheckElementVisible(const DeepQuery& where, bool visible) {
    return CheckJsResultAt(kDownloadsPageTabId, where,
                           R"(el => {
                               rect = el.getBoundingClientRect();
                               return rect.height * rect.width > 0;
                            })",
                           Eq(visible));
  }

  // Finds the first download item on the page and clicks a button on the item.
  auto ClickTopmostItemButton(const std::string& button_selector) {
    const DeepQuery path_to_button = PathToTopmostItemElement(button_selector);
    return Steps(CheckElementVisible(path_to_button, true),
                 ExecuteJsAt(kDownloadsPageTabId, path_to_button, kClickFn));
  }

  // Presses the clear all button in the toolbar.
  auto PressClearAll() {
    const DeepQuery kPathToClearAllButton{
        "downloads-manager",
        "downloads-toolbar",
        "#clearAll",
    };
    return ExecuteJsAt(kDownloadsPageTabId, kPathToClearAllButton, kClickFn);
  }

  // Waits for `num` download items to be present in the downloads list. Note
  // that iron-list keeps removed elements in the DOM (for possible reuse) so
  // the number of download items present is not necessarily the number of list
  // elements that are actually displayed, if items have been removed.
  auto WaitForDownloadItems(int num) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kReadyEvent);
    const DeepQuery kPathToDownloadsList{
        "downloads-manager",
        "#downloadsList",
    };
    StateChange item_count_matches;
    item_count_matches.type = StateChange::Type::kExistsAndConditionTrue;
    item_count_matches.where = kPathToDownloadsList;
    item_count_matches.test_function = base::StringPrintf(
        "el => el.querySelectorAll('downloads-item').length === %d", num);
    item_count_matches.event = kReadyEvent;
    return WaitForStateChange(kDownloadsPageTabId, item_count_matches);
  }

  // Waits for the "no downloads" splash screen to be displayed.
  auto WaitForNoDownloads() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kReadyEvent);
    const DeepQuery kPathToNoDownloads{
        "downloads-manager",
        "#no-downloads:not([hidden])",
    };
    StateChange no_downloads_visible;
    no_downloads_visible.type = StateChange::Type::kExists;
    no_downloads_visible.where = kPathToNoDownloads;
    no_downloads_visible.event = kReadyEvent;
    return WaitForStateChange(kDownloadsPageTabId, no_downloads_visible);
  }

  // Downloads a normal file.
  auto DownloadTestFile() {
    return Do(base::BindLambdaForTesting([&]() {
      DownloadAndWait(browser(),
                      embedded_test_server()->GetURL(base::StrCat(
                          {"/", DownloadTestBase::kDownloadTest1Path})));
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitest, DownloadItemsAppear) {
  RunTestSequence(OpenDownloadsPage(),      //
                  DownloadTestFile(),       //
                  WaitForDownloadItems(1),  //
                  DownloadTestFile(),       //
                  WaitForDownloadItems(2));
}

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitest,
                       QuickRemoveDownloadItem) {
  RunTestSequence(
      OpenDownloadsPage(),      //
      DownloadTestFile(),       //
      WaitForDownloadItems(1),  //
      // A normal, completed download should have quick action buttons but not
      // a menu button.
      CheckElementVisible(PathToTopmostItemElement("#more-actions"), false),
      ClickTopmostItemButton("#quick-remove"),  //
      WaitForNoDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadsPageInteractiveUitest, ClearAll) {
  RunTestSequence(OpenDownloadsPage(),      //
                  DownloadTestFile(),       //
                  DownloadTestFile(),       //
                  WaitForDownloadItems(2),  //
                  PressClearAll(),          //
                  WaitForNoDownloads());
}

}  // namespace
