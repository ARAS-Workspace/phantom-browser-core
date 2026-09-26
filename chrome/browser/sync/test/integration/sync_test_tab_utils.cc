// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test_tab_utils.h"

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_group.h"

using sync_datatype_helper::test;

namespace sync_test_tab_utils {

namespace {

Browser* GetBrowserOrDie() {
  CHECK_EQ(test()->num_clients(), 1)
      << "Tab utils support only single-client tests";
  Browser* browser = test()->GetBrowser(/*index=*/0);
  CHECK(browser);
  return browser;
}

}  // namespace

std::optional<size_t> OpenNewTab(const GURL& url) {
  content::WebContents* web_contents = nullptr;
  size_t tab_index = 0;
  TabStripModel* tab_strip = GetBrowserOrDie()->tab_strip_model();
  tab_index = tab_strip->count();
  web_contents = chrome::AddAndReturnTabAt(GetBrowserOrDie(), url, tab_index,
                                           /*foreground=*/true);
  CHECK(web_contents);
  CHECK(WaitForLoadStop(web_contents)) << "Failed to load URL: " << url;
  return tab_index;
}

tab_groups::LocalTabGroupID CreateGroupFromTab(
    size_t tab_index,
    std::string_view title,
    tab_groups::TabGroupColorId color) {
  std::optional<tab_groups::LocalTabGroupID> local_group_id;
  TabStripModel* tab_strip = GetBrowserOrDie()->tab_strip_model();
  local_group_id = tab_strip->AddToNewGroup({static_cast<int>(tab_index)});
  CHECK(local_group_id.has_value());
  UpdateTabGroupVisualData(local_group_id.value(), title, color);
  return local_group_id.value();
}

bool IsTabGroupOpen(const tab_groups::LocalTabGroupID& local_group_id) {
  TabStripModel* tab_strip = GetBrowserOrDie()->tab_strip_model();
  return tab_strip->group_model()->ContainsTabGroup(local_group_id);
}

void UpdateTabGroupVisualData(const tab_groups::LocalTabGroupID& local_group_id,
                              const std::string_view& title,
                              tab_groups::TabGroupColorId color) {
  TabStripModel* tab_strip = GetBrowserOrDie()->tab_strip_model();
  TabGroup* model_tab_group =
      tab_strip->group_model()->GetTabGroup(local_group_id);
  CHECK(model_tab_group);
  tab_strip->ChangeTabGroupVisuals(
      local_group_id,
      tab_groups::TabGroupVisualData(base::UTF8ToUTF16(title), color),
      /*is_customized=*/true);
}

}  // namespace sync_test_tab_utils
