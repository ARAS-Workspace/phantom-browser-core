// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/test/mock_callback.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

// Observes TabStripModelChange::kRemoved notifications and records the
// WebContents remove reason and the TabModel detach reason.
class WebContentsRemovedObserver : public TabStripModelObserver {
 public:
  WebContentsRemovedObserver() = default;
  WebContentsRemovedObserver(const WebContentsRemovedObserver&) = delete;
  WebContentsRemovedObserver& operator=(const WebContentsRemovedObserver&) =
      delete;
  ~WebContentsRemovedObserver() override = default;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kRemoved) {
      const TabStripModelChange::RemovedTab& removed_tab =
          change.GetRemove()->contents[0];
      remove_reason_ = removed_tab.remove_reason;
      tab_detach_reason_ = removed_tab.tab_detach_reason;
    }
  }

  std::optional<TabRemovedReason> remove_reason() const {
    return remove_reason_;
  }
  std::optional<tabs::TabInterface::DetachReason> tab_detach_reason() const {
    return tab_detach_reason_;
  }

 private:
  std::optional<TabRemovedReason> remove_reason_;
  std::optional<tabs::TabInterface::DetachReason> tab_detach_reason_;
};

}  // namespace

// The local third party NTP is the NTP with the DuckDuckGo fallback default
// search provider.
class WebUiThirdPartyNtpBrowserTest : public InProcessBrowserTest {
 private:
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
};

// Verify that the local third party NTP uses process-per-site.
IN_PROC_BROWSER_TEST_F(WebUiThirdPartyNtpBrowserTest, ProcessPerSite) {
  std::vector<content::WebContents*> tabs;

  // Open a few NTPs.
  for (size_t i = 0; i < 3; i++) {
    content::WebContentsAddedObserver tab_observer;
    chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

    // Wait for the new tab.
    auto* tab = tab_observer.GetWebContents();
    ASSERT_TRUE(WaitForLoadStop(tab));

    // Sanity check: the NTP should be the local third party NTP.
    EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL,
              EvalJs(tab, "window.location.href",
                     content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                     /*world_id=*/1));

    tabs.push_back(tab);
  }

  // Verify that all NTPs share a process.
  for (size_t i = 1; i < tabs.size(); i++) {
    EXPECT_EQ(tabs[0]->GetPrimaryMainFrame()->GetProcess(),
              tabs[i]->GetPrimaryMainFrame()->GetProcess());
  }
}

// Currently it is possible to load the NTP absent a TabModel in an app window
// context. Assert the NTP loads without a TabModel successfully.
// TODO(crbug.com/378475391): This can be removed once an owning TabModel is
// guaranteed to be always present and fixed for the lifetime of the NTP.
IN_PROC_BROWSER_TEST_F(WebUiThirdPartyNtpBrowserTest,
                       LoadsSuccessfullyWithoutTabModel) {
  // Add a new about:blank tab to the browser tab strip.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), 1, true);
  tabs::TabInterface* initial_tab = tab_strip_model->GetTabAtIndex(1);
  EXPECT_EQ(2, tab_strip_model->count());

  base::MockCallback<tabs::TabInterface::WillDetach> tab_detached_callback;
  base::CallbackListSubscription tab_subscription =
      initial_tab->RegisterWillDetach(tab_detached_callback.Get());
  WebContentsRemovedObserver removed_observer;
  tab_strip_model->AddObserver(&removed_observer);

  // Extract the about:blank tab WebContents while destroying its TabModel.
  EXPECT_CALL(tab_detached_callback,
              Run(tab_strip_model->GetTabAtIndex(1),
                  tabs::TabInterface::DetachReason::kDelete));
  std::unique_ptr<content::WebContents> extracted_contents =
      tab_strip_model->DetachWebContentsAtForInsertion(1);
  EXPECT_EQ(TabRemovedReason::kInsertedIntoOtherTabStrip,
            removed_observer.remove_reason());
  EXPECT_EQ(tabs::TabInterface::DetachReason::kDelete,
            removed_observer.tab_detach_reason());

  // Load the NTP into the detached tab. The NTP should load without crashing.
  EXPECT_TRUE(content::NavigateToURL(extracted_contents.get(),
                                     chrome::ChromeUINewTabURLAsGURL()));

  // Re-insert the tab into the tab strip.
  tab_strip_model->AppendWebContents(std::move(extracted_contents), true);
}

// Currently it is possible to load the NTP in an app window context that
// supports re-parenting the tab into a normal browser window. Assert the NTP
// supports such TabModel transitions.
// TODO(crbug.com/378475391): This can be removed once an owning TabModel is
// guaranteed to be always present and fixed for the lifetime of the NTP.
IN_PROC_BROWSER_TEST_F(WebUiThirdPartyNtpBrowserTest, HandlesTabModelChanges) {
  // Add a new NTP tab to the browser tab strip.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  chrome::AddTabAt(browser(), chrome::ChromeUINewTabURLAsGURL(), 1, true);
  tabs::TabInterface* initial_tab = tab_strip_model->GetTabAtIndex(1);
  EXPECT_EQ(2, tab_strip_model->count());

  base::MockCallback<tabs::TabInterface::WillDetach> tab_detached_callback;
  base::CallbackListSubscription tab_subscription =
      initial_tab->RegisterWillDetach(tab_detached_callback.Get());
  WebContentsRemovedObserver removed_observer;
  tab_strip_model->AddObserver(&removed_observer);

  // Extract the NTP WebContents while destroying its TabModel, simulating what
  // occurs during a browser-type transition.
  EXPECT_CALL(tab_detached_callback,
              Run(tab_strip_model->GetTabAtIndex(1),
                  tabs::TabInterface::DetachReason::kDelete));
  std::unique_ptr<content::WebContents> extracted_contents =
      tab_strip_model->DetachWebContentsAtForInsertion(1);
  EXPECT_EQ(TabRemovedReason::kInsertedIntoOtherTabStrip,
            removed_observer.remove_reason());
  EXPECT_EQ(tabs::TabInterface::DetachReason::kDelete,
            removed_observer.tab_detach_reason());

  // Re-insert the tab into the tab strip, creating a new TabModel.
  tab_strip_model->AppendWebContents(std::move(extracted_contents), true);
}
