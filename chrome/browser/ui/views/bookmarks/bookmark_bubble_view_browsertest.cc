// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "url/url_constants.h"

namespace {
const char kTestBookmarkURL[] = "http://www.google.com";
}

class BookmarkBubbleViewMigrationBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    bookmark_model_ =
        BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    bookmark_node_ = bookmarks::AddIfNotBookmarked(
        bookmark_model_, GURL(kTestBookmarkURL), std::u16string());

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(kTestBookmarkURL)));
  }

  void TearDownOnMainThread() override {
    if (BookmarkBubbleView::bookmark_bubble()) {
      views::test::WidgetDestroyedWaiter destroyed_waiter(
          BookmarkBubbleView::bookmark_bubble()->GetWidget());
      BookmarkBubbleView::bookmark_bubble()->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kUnspecified);
      destroyed_waiter.Wait();
    }
    bookmark_model_ = nullptr;
    bookmark_node_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void CreateBubbleView(bool already_bookmarked = true) {
    BrowserWindow::FromBrowser(browser())->ShowBookmarkBubble(
        GURL(kTestBookmarkURL), already_bookmarked);
  }

  bookmarks::BookmarkModel* GetBookmarkModel() { return bookmark_model_; }
  const bookmarks::BookmarkNode* GetBookmark() { return bookmark_node_; }

  views::View* GetViewInBookmarkBubble(ui::ElementIdentifier id) {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(
            BookmarkBubbleView::bookmark_bubble()->GetAnchorView());
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        id, context);
  }

 protected:
  raw_ptr<const bookmarks::BookmarkNode> bookmark_node_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  base::test::ScopedFeatureList test_features_;
};

// The bubble edits the bookmark of one specific tab. Closing that tab used to
// leave the bubble on screen, still holding on to the destroyed tab's state;
// pressing "Done" on the orphaned bubble then crashed the browser. See
// https://crbug.com/526451600.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       BubbleClosesWhenTabIsClosed) {
  // A second tab keeps the browser window around once the bookmarked tab goes
  // away.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  TabStripModel* const tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(2, tab_strip->count());

  tab_strip->ActivateTabAt(0);
  ASSERT_EQ(0, tab_strip->active_index());

  CreateBubbleView();
  ASSERT_TRUE(BookmarkBubbleView::bookmark_bubble());

  tab_strip->CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       BubbleClosesWhenTabIsBackgrounded) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  TabStripModel* const tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(2, tab_strip->count());

  tab_strip->ActivateTabAt(0);
  ASSERT_EQ(0, tab_strip->active_index());

  CreateBubbleView();
  ASSERT_TRUE(BookmarkBubbleView::bookmark_bubble());

  tab_strip->ActivateTabAt(1);
  EXPECT_FALSE(BookmarkBubbleView::bookmark_bubble());
}

// Verifies that the sync promo is not displayed for a signed in user.
// TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
// deleted from the codebase. See ConsentLevel::kSync documentation for
// details.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       SyncPromoSignedIn) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->GetProfile()),
      "fake_username@gmail.com", signin::ConsentLevel::kSync);
  CreateBubbleView();
  EXPECT_FALSE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}

// Verifies that the sync promo is displayed for a user that is not signed in.
// TODO(crbug.com/491520553): This test is highly brittle in the
// InProcessBrowserTest environment. It relies on injecting a TestSyncService
// before the real IdentityManager and SyncService fully initialize, which
// causes timing and race conditions when attempting to trick the native UI
// layer into showing the "Not Signed In" promo state. See the migration KI for
// details on mocking SyncService.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewMigrationBrowserTest,
                       DISABLED_SyncPromoNotSignedIn) {
  CreateBubbleView();
  views::View* footnote =
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting();
  EXPECT_TRUE(footnote);
}

class BookmarkBubbleViewWithAccountBookmarksBrowserTest
    : public BookmarkBubbleViewMigrationBrowserTest {
 public:
  BookmarkBubbleViewWithAccountBookmarksBrowserTest() {
    test_features_.InitAndEnableFeature(
        switches::kSyncEnableBookmarksInTransportMode);
  }
};

// Verifies that the bookmark bubble correctly instantiates a combobox that
// separates account bookmarks and local bookmarks with headers. It also
// verifies that RecentlyUsedFoldersComboModel correctly reports those as
// "title" items and that ComboboxMenuModel correctly translates that to a
// TYPE_TITLE item that can be rendered differently when the popup menu is
// displayed within the browser UI environment.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewWithAccountBookmarksBrowserTest,
                       ComboboxUsesTitlesForHeaders) {
  GetBookmarkModel()->CreateAccountPermanentFolders();
  CreateBubbleView();

  views::View* folder_field = GetViewInBookmarkBubble(kBookmarkFolderFieldId);
  ASSERT_TRUE(folder_field);
  views::Combobox* combobox = views::AsViewClass<views::Combobox>(folder_field);
  ASSERT_TRUE(combobox);

  EXPECT_EQ(ui::MenuModel::TYPE_TITLE,
            combobox->menu_model_for_testing()->GetTypeAt(0));
}
