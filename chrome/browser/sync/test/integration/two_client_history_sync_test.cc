// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/history_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

using bookmarks_helper::StoreType;
using history_helper::HasVisitDuration;
using history_helper::UrlIs;
using history_helper::VisitRowHasDuration;
using testing::AllOf;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace {

class TwoClientHistorySyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  TwoClientHistorySyncTest() : SyncTest(TWO_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features;
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
      enabled_features.push_back(switches::kSyncEnableBookmarksInTransportMode);
    }
    // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    features_.InitWithFeatures(
        enabled_features, /*disabled_features=*/{features::kHttpsUpgrades});
  }
  ~TwoClientHistorySyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    SyncTest::SetUpOnMainThread();
  }

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    // SyncTest doesn't create any tabs in the profiles/browsers it creates.
    // Create an "empty" tab here, so that NavigateToURL() will have a non-null
    // WebContents to navigate in.
    for (int i = 0; i < num_clients(); ++i) {
      if (!AddTabAtIndexToBrowser(GetBrowser(i), 0, GURL("about:blank"),
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL)) {
        return false;
      }
    }

    return true;
  }

  void NavigateToURL(
      int profile_index,
      const GURL& url,
      ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED) {
    content::NavigationController::LoadURLParams params(url);
    params.transition_type = transition;
    content::NavigateToURLBlockUntilNavigationsComplete(
        GetActiveWebContents(profile_index), params, 1);

    // Ensure the navigation succeeded (i.e. whatever test URL was passed in was
    // actually valid).
    ASSERT_EQ(200, GetActiveWebContents(profile_index)
                       ->GetController()
                       .GetLastCommittedEntry()
                       ->GetHttpStatusCode());
  }

  [[nodiscard]] bool WaitForServerHistory(
      testing::Matcher<std::vector<sync_pb::HistorySpecifics>> matcher) {
    return history_helper::ServerHistoryMatchChecker(matcher).Wait();
  }

  [[nodiscard]] bool WaitForLocalHistory(
      int profile_index,
      const std::map<GURL, testing::Matcher<std::vector<history::VisitRow>>>&
          matchers) {
    return history_helper::LocalHistoryMatchChecker(
               profile_index, GetSyncService(profile_index), matchers)
        .Wait();
  }

  content::WebContents* GetActiveWebContents(int profile_index) {
    // Note: chrome_test_utils::GetActiveWebContents() doesn't work, since it
    // uses the profile created by InProcessBrowserTest, not the profiles from
    // SyncTest.
    return GetBrowser(profile_index)->tab_strip_model()->GetActiveWebContents();
  }

  StoreType GetStoreType() {
    return GetSetupSyncMode() == SyncTest::SetupSyncMode::kSyncTransportOnly
               ? StoreType::kAccountStore
               : StoreType::kLocalOrSyncableStore;
  }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(,
                         TwoClientHistorySyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

// Very simple test; its main reason for existence is that it's the only E2E
// test covering history.
IN_PROC_BROWSER_TEST_P(TwoClientHistorySyncTest, E2E_ENABLED(SyncsUrl)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  // Use a randomized URL to prevent test collisions.
  const std::u16string kHistoryUrl = base::ASCIIToUTF16(base::StringPrintf(
      "http://www.add-history.google.com/%s",
      base::Uuid::GenerateRandomV4().AsLowercaseString().c_str()));
  ASSERT_TRUE(SetupSync());

  // Populate one client with a URL, wait for it to sync to the other.
  GURL new_url(kHistoryUrl);
  history_helper::AddUrlToHistory(0, new_url);
  EXPECT_TRUE(WaitForLocalHistory(1, {{new_url, SizeIs(1)}}));
}

IN_PROC_BROWSER_TEST_P(TwoClientHistorySyncTest, SyncsVisitForBookmarkedUrl) {
  GURL bookmark_url("http://www.bookmark.google.com/");
  GURL bookmark_icon_url("http://www.bookmark.google.com/favicon.ico");
  ASSERT_TRUE(SetupSync());

  // Create a bookmark and wait for it to sync.
  const bookmarks::BookmarkNode* node = bookmarks_helper::AddURL(
      0, bookmarks_helper::IndexedURLTitle(0), bookmark_url, GetStoreType());
  bookmarks_helper::SetFavicon(0, node, bookmark_icon_url,
                               bookmarks_helper::CreateFavicon(SK_ColorWHITE),
                               bookmarks_helper::FROM_UI);
  ASSERT_TRUE(bookmarks_helper::BookmarksMatchChecker().Wait());

  // A row in the DB for client 1 should have been created as a result of
  // syncing the bookmark.
  history::URLRow row;
  ASSERT_TRUE(history_helper::GetUrlFromClient(1, bookmark_url, &row));

  // Now, add a visit for client 0 to the bookmark URL and sync it over - this
  // should not cause a crash.
  history_helper::AddUrlToHistory(0, bookmark_url);

  ASSERT_TRUE(WaitForLocalHistory(0, {{bookmark_url, SizeIs(1)}}));
  EXPECT_TRUE(WaitForLocalHistory(1, {{bookmark_url, SizeIs(1)}}));
}

}  // namespace
