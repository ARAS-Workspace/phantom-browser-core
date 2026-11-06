// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/groups/recent_activity_bubble_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_bubble_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/collaboration/public/features.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/unique_widget_ptr.h"

using collaboration::messaging::ActivityLogItem;
using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::MessageAttribution;
using collaboration::messaging::RecentActivityAction;
using collaboration::messaging::TabMessageMetadata;
using data_sharing::GroupMember;

namespace tab_groups {
namespace {

// Copied from
// components/collaboration/internal/messaging/messaging_backend_service_impl.cc
RecentActivityAction GetRecentActivityActionFromCollaborationEvent(
    CollaborationEvent event) {
  switch (event) {
    case CollaborationEvent::TAB_ADDED:
    case CollaborationEvent::TAB_UPDATED:
      return RecentActivityAction::kFocusTab;
    case CollaborationEvent::TAB_REMOVED:
      return RecentActivityAction::kReopenTab;
    case CollaborationEvent::TAB_GROUP_ADDED:
    case CollaborationEvent::TAB_GROUP_REMOVED:
      return RecentActivityAction::kNone;
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
      return RecentActivityAction::kOpenTabGroupEditDialog;
    case CollaborationEvent::COLLABORATION_ADDED:
    case CollaborationEvent::COLLABORATION_REMOVED:
      return RecentActivityAction::kNone;
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      return RecentActivityAction::kManageSharing;
    case CollaborationEvent::UNDEFINED:
      return RecentActivityAction::kNone;
  }
}

// Helper to create mock log items.
collaboration::messaging::ActivityLogItem CreateMockActivityItem(
    std::string_view name,
    std::string_view avatar_url,
    std::string_view last_url,
    std::string_view last_url_description,
    std::string_view time_delta,
    tab_groups::CollaborationEvent event,
    bool is_self) {
  GroupMember trig_member;
  trig_member.given_name = name;
  trig_member.avatar_url = GURL(avatar_url);

  GroupMember aff_member;
  aff_member.given_name = name;
  aff_member.avatar_url = GURL(avatar_url);

  TabMessageMetadata tab_metadata;
  tab_metadata.last_known_url = last_url;

  MessageAttribution attribution;
  attribution.triggering_user = trig_member;
  attribution.affected_user = aff_member;
  attribution.tab_metadata = tab_metadata;

  ActivityLogItem item;
  item.collaboration_event = event;
  item.title_text = base::UTF8ToUTF16(name);
  item.description_text = base::UTF8ToUTF16(last_url_description);
  item.time_delta_text = base::UTF8ToUTF16(time_delta);
  item.show_favicon = true;
  item.action =
      GetRecentActivityActionFromCollaborationEvent(item.collaboration_event);
  item.activity_metadata = attribution;
  return item;
}

// Helper to create mock log with all types of events.
std::vector<collaboration::messaging::ActivityLogItem>
CreateMockActivityLogWithAllTypes() {
  std::vector<ActivityLogItem> result;

  result.emplace_back(CreateMockActivityItem(
      "You changed a tab", "https://www.google.com/avatar/1",
      "https://www.google.com/1", "airbnb.com", "5h ago",
      tab_groups::CollaborationEvent::TAB_UPDATED, true));

  result.emplace_back(CreateMockActivityItem(
      "Shirley changed a tab", "https://www.google.com/avatar/2",
      "https://www.google.com/2", "hotels.com", "4h ago",
      tab_groups::CollaborationEvent::TAB_UPDATED, false));

  result.emplace_back(CreateMockActivityItem(
      "Elisa removed a tab", "https://www.google.com/avatar/3",
      "https://www.google.com/3", "expedia.com", "6h ago",
      tab_groups::CollaborationEvent::TAB_REMOVED, false));

  result.emplace_back(CreateMockActivityItem(
      "Shirley joined the group", "https://www.google.com/avatar/2",
      "https://www.google.com/2", "shirleys-email", "8h ago",
      tab_groups::CollaborationEvent::COLLABORATION_MEMBER_ADDED, false));

  result.emplace_back(CreateMockActivityItem(
      "Elisa added a tab", "https://www.google.com/avatar/3",
      "https://www.google.com/3", "expedia.com", "2d ago",
      tab_groups::CollaborationEvent::TAB_ADDED, false));

  return result;
}

// Helper to create a list of n items where the contents are not
// important for verification.
std::vector<ActivityLogItem> CreateMockActivityLog(int n) {
  std::vector<ActivityLogItem> result;
  for (int i = 0; i < n; i++) {
    // Choose random values to populate list.
    result.emplace_back(CreateMockActivityItem(
        "You changed a tab", "https://www.google.com/avatar/1",
        "https://www.google.com/1", "airbnb.com", "5m ago",
        tab_groups::CollaborationEvent::TAB_UPDATED, true));
  }
  return result;
}
}  // namespace

class RecentActivityBubbleDialogViewBrowserTest : public DialogBrowserTest {
 public:
  RecentActivityBubbleDialogViewBrowserTest() {
    features_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature,
         collaboration::features::kCollaborationMessaging},
        {});
  }
  void ShowUi(const std::string& name) override {
    if (name == "Empty") {
      ShowLog({});
    } else if (name == "WithOneItem") {
      ShowLog(CreateMockActivityLog(1));
    } else if (name == "WithFullList") {
      ShowLog(CreateMockActivityLog(5));
    } else if (name == "WithTooManyItems") {
      ShowLog(CreateMockActivityLog(10));
    } else if (name == "ForCurrentTab") {
      ShowLogForCurrentTab(CreateMockActivityLog(5));
    }
  }

  void ShowLog(std::vector<ActivityLogItem> activity_log) {
    // Anchor to top container for tests.
    views::View* anchor_view =
        BrowserView::GetBrowserViewForBrowser(browser())->top_container();

    EXPECT_NE(nullptr, BubbleCoordinator());
    EXPECT_EQ(nullptr, BubbleCoordinator()->GetBubble());

    BubbleCoordinator()->Show(views::BubbleAnchor(anchor_view),
                              browser()->tab_strip_model()->GetWebContentsAt(0),
                              activity_log, browser()->GetProfile());
  }

  void ShowLogForCurrentTab(std::vector<ActivityLogItem> activity_log) {
    // Anchor to top container for tests.
    views::View* anchor_view =
        BrowserView::GetBrowserViewForBrowser(browser())->top_container();

    EXPECT_NE(nullptr, BubbleCoordinator());
    EXPECT_EQ(nullptr, BubbleCoordinator()->GetBubble());

    BubbleCoordinator()->ShowForCurrentTab(
        views::BubbleAnchor(anchor_view),
        browser()->tab_strip_model()->GetWebContentsAt(0), {}, activity_log,
        browser()->GetProfile());
  }

  bool VerifyUi() override {
    EXPECT_NE(nullptr, BubbleCoordinator());

    EXPECT_TRUE(BubbleCoordinator()->IsShowing());
    EXPECT_NE(nullptr, BubbleCoordinator()->GetBubble());
    auto* bubble = BubbleCoordinator()->GetBubble();
    auto children = bubble->children();

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();

    // Containers are always created
    EXPECT_TRUE(bubble->tab_activity_container());
    EXPECT_TRUE(bubble->group_activity_container());

    // All dialogs have 4 children, except for empty state dialog, which
    // also contains the label for the empty state.
    if (test_name == "InvokeUi_Empty") {
      EXPECT_EQ(6u, children.size());
    } else {
      EXPECT_EQ(5u, children.size());
    }

    // Tab container empty and hidden.
    EXPECT_FALSE(bubble->tab_activity_container()->GetVisible());
    EXPECT_EQ(0u, bubble->tab_activity_container()->children().size());

    if (test_name == "InvokeUi_Empty") {
      // Group container empty and hidden.
      EXPECT_FALSE(bubble->group_activity_container()->GetVisible());
      EXPECT_EQ(0u, bubble->group_activity_container()->children().size());
    } else if (test_name == "InvokeUi_WithOneItem") {
      // Group container is visible with one child.
      EXPECT_TRUE(bubble->group_activity_container()->GetVisible());
      EXPECT_EQ(1u, bubble->group_activity_container()->children().size());
    } else {
      // All other tests expect a complete list of 5 items in the group
      // container.
      EXPECT_TRUE(bubble->group_activity_container()->GetVisible());
      EXPECT_EQ(5u, bubble->group_activity_container()->children().size());
    }

    return true;
  }

  void DismissUi() override { BubbleCoordinator()->Hide(); }

  RecentActivityBubbleCoordinator* BubbleCoordinator() {
    return RecentActivityBubbleCoordinator::From(browser());
  }

 private:
  views::UniqueWidgetPtr anchor_widget_;
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       InvokeUi_Empty) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       InvokeUi_WithOneItem) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       InvokeUi_WithFullList) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       InvokeUi_WithTooManyItems) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       InvokeUi_ForCurrentTab) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       ShowsAllTypes) {
  auto activity_log = CreateMockActivityLogWithAllTypes();
  ShowLog(activity_log);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

#if BUILDFLAG(IS_MAC)
  // Initial caps on Mac.
  EXPECT_EQ(bubble->GetTitleForTesting(), u"Recent Activity");
#else
  EXPECT_EQ(bubble->GetTitleForTesting(), u"Recent activity");
#endif

  EXPECT_EQ(bubble->GetRowForTesting(0)->GetAccessibleName(),
            u"You changed a tab");
  EXPECT_EQ(bubble->GetRowForTesting(0)->GetAccessibleDescription(),
            u"airbnb.com \u2022 5h ago");

  EXPECT_EQ(bubble->GetRowForTesting(1)->GetAccessibleName(),
            u"Shirley changed a tab");
  EXPECT_EQ(bubble->GetRowForTesting(1)->GetAccessibleDescription(),
            u"hotels.com \u2022 4h ago");

  EXPECT_EQ(bubble->GetRowForTesting(2)->GetAccessibleName(),
            u"Elisa removed a tab");
  EXPECT_EQ(bubble->GetRowForTesting(2)->GetAccessibleDescription(),
            u"expedia.com \u2022 6h ago");

  EXPECT_EQ(bubble->GetRowForTesting(3)->GetAccessibleName(),
            u"Shirley joined the group");
  EXPECT_EQ(bubble->GetRowForTesting(3)->GetAccessibleDescription(),
            u"shirleys-email \u2022 8h ago");

  EXPECT_EQ(bubble->GetRowForTesting(4)->GetAccessibleName(),
            u"Elisa added a tab");
  EXPECT_EQ(bubble->GetRowForTesting(4)->GetAccessibleDescription(),
            u"expedia.com \u2022 2d ago");
}

}  // namespace tab_groups
