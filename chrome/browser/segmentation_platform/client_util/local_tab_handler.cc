// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/client_util/local_tab_handler.h"

#include "base/time/time.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"

namespace segmentation_platform::processing {

namespace {

GURL GetLocalTabURL(const TabFetcher::Tab& tab) {
  if (tab.webcontents) {
    return tab.webcontents->GetURL();
  }
  return GURL();
}

// Returns the time since last time the tab was modified.
base::TimeDelta GetLocalTimeSinceModified(const TabFetcher::Tab& tab) {
  base::Time last_modified_timestamp;
  if (tab.webcontents) {
    auto* last_entry = tab.webcontents->GetController().GetLastCommittedEntry();
    if (last_entry) {
      last_modified_timestamp = last_entry->GetTimestamp();
    }
  }
  return base::Time::Now() - last_modified_timestamp;
}

// Returns a list of all tabs from tab strip model.
std::vector<TabFetcher::TabEntry> FetchTabs(const Profile* profile) {
  std::vector<TabFetcher::TabEntry> tabs;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [profile, &tabs](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile) {
          return true;
        }
        const TabStripModel* const tab_strip_model =
            browser->GetTabStripModel();
        for (int i = 0; i < tab_strip_model->count(); ++i) {
          content::WebContents* const web_contents =
              tab_strip_model->GetWebContentsAt(i);
          auto* const tab_delegate =
              BrowserSyncedTabDelegate::FromWebContents(web_contents);
          tabs.emplace_back(tab_delegate->GetSessionId(), web_contents,
                            nullptr);
        }
        return true;
      });
  return tabs;
}

}  // namespace

LocalTabHandler::LocalTabHandler(
    sync_sessions::SessionSyncService* session_sync_service,
    Profile* profile)
    : TabFetcher(session_sync_service), profile_(profile) {}

LocalTabHandler::~LocalTabHandler() = default;

bool LocalTabHandler::FillAllLocalTabsFromTabModel(
    std::vector<TabEntry>& tabs) {
  tabs = FetchTabs(profile_);
  return true;
}

TabFetcher::Tab LocalTabHandler::FindLocalTab(const TabEntry& entry) {
  TabFetcher::Tab result;
  // Fetch all tabs and verify if the `entry` is still valid.
  auto all_local_tabs = FetchTabs(profile_);
  for (auto& tab : all_local_tabs) {
    if (tab.tab_id == entry.tab_id) {
      result.webcontents =
          reinterpret_cast<content::WebContents*>(tab.web_contents_data.get());
      result.tab_android =
          reinterpret_cast<TabAndroid*>(tab.tab_android_data.get());
      result.time_since_modified = GetLocalTimeSinceModified(result);
      result.tab_url = GetLocalTabURL(result);
      break;
    }
  }
  return result;
}

LocalTabSource::LocalTabSource(
    sync_sessions::SessionSyncService* session_sync_service,
    TabFetcher* tab_fetcher)
    : TabSessionSource(session_sync_service, tab_fetcher) {}

LocalTabSource::~LocalTabSource() = default;

void LocalTabSource::AddLocalTabInfo(
    const TabFetcher::Tab& tab,
    FeatureProcessorState& feature_processor_state,
    Tensor& inputs) {
  inputs[TabSessionSource::kInputLocalTabTimeSinceModified] =
      ProcessedValue::FromFloat(
          BucketizeExp(GetLocalTimeSinceModified(tab).InSeconds(), /*max_buckets*/50));
}

}  // namespace segmentation_platform::processing
