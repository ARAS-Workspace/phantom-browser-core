// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_window_sub_menu_model.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/base/base_window.h"
#include "ui/gfx/text_elider.h"

namespace {

class ExistingWindowSubMenuModelTest : public InProcessBrowserTest {
 public:
  ExistingWindowSubMenuModelTest() = default;

  Profile* profile() { return browser()->GetProfile(); }

 protected:
  Browser* CreateTestBrowser(bool incognito, bool popup) {
    Profile* profile = incognito
                           ? browser()->GetProfile()->GetPrimaryOTRProfile(
                                 /*create_if_needed=*/true)
                           : browser()->GetProfile();
    BrowserWindowInterface::Type type =
        popup ? BrowserWindowInterface::TYPE_POPUP
              : BrowserWindowInterface::TYPE_NORMAL;

    Browser* browser =
        CreateBrowserWindow(BrowserWindowCreateParams(
                                type, profile, /*from_user_gesture=*/true))
            ->GetBrowserForMigrationOnly();
    ActivateBrowser(browser);
    // Self deleting.
    return browser;
  }
  void AddTabWithTitle(Browser* browser, std::string title) {
    chrome::AddTabAt(browser, GURL("about:blank"), /*index=*/-1,
                     /*foreground=*/true);

    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    contents->UpdateTitleForEntry(contents->GetController().GetActiveEntry(),
                                  base::ASCIIToUTF16(title));
  }

  void CheckBrowserTitle(const std::u16string& title,
                         const std::string& expected_title,
                         const int expected_num_tabs) {
    const std::u16string expected_title16 = base::ASCIIToUTF16(expected_title);

    // Check the suffix, which should always show if there are multiple tabs.
    if (expected_num_tabs > 1) {
      std::ostringstream oss;
      oss << " and " << expected_num_tabs - 1;
      oss << ((expected_num_tabs == 2) ? " other tab" : " other tabs");
      const std::u16string expected_suffix16 = base::ASCIIToUTF16(oss.str());

      // Not case sensitive, since MacOS uses title case.
      EXPECT_TRUE(base::EndsWith(title, expected_suffix16,
                                 base::CompareCase::INSENSITIVE_ASCII));
    }

    // Either the title contains the whole tab title, or it was elided.
    if (!base::StartsWith(title, expected_title16,
                          base::CompareCase::SENSITIVE)) {
      // Check that the title before being elided matches the tab title.
      std::vector<std::u16string> tokens =
          SplitString(title, gfx::kEllipsisUTF16, base::KEEP_WHITESPACE,
                      base::SPLIT_WANT_NONEMPTY);
      EXPECT_TRUE(base::StartsWith(expected_title16, tokens[0],
                                   base::CompareCase::SENSITIVE));

      // Title should always have at least a few characters.
      EXPECT_GE(tokens[0].size(), 3u);
    }
  }

  // TODO(crbug.com/480103891): We should not be faking browser activation state
  // via indirect means (such as direct calls to `DidBecomeActive()`). We should
  // instead convert this to an interactive browser test and directly activate
  // the browser's backing ui::BaseWindow.
  void ActivateBrowser(BrowserWindowInterface* browser) {
    ui_test_utils::DeprecatedFakeActivateBrowser(browser);
  }
};

// Ensure that the move to existing window menu only appears when another window
// of the current profile exists.
IN_PROC_BROWSER_TEST_F(ExistingWindowSubMenuModelTest, ShouldShowSubmenu) {
  // Shouldn't show menu for one window.
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));

  // Add another browser, and make sure we do show the menu now.
  Browser* browser_2(CreateTestBrowser(false, false));
  ASSERT_TRUE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));

  // Close the window, so the menu does not show anymore.
  CloseBrowserSynchronously(browser_2);
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));
}

// Ensure we only show the menu on incognito when at least one other incognito
// window exists.
IN_PROC_BROWSER_TEST_F(ExistingWindowSubMenuModelTest,
                       ShouldShowSubmenuIncognito) {
  // Shouldn't show menu for one window.
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(
      browser()->GetProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true)));

  // Create an incognito browser. We shouldn't show the menu, because we only
  // move tabs between windows of the same profile.
  Browser* incognito_browser_1(CreateTestBrowser(true, false));
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(
      incognito_browser_1->GetProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true)));

  // Add another incognito browser, and make sure we do show the menu now.
  Browser* incognito_browser_2(CreateTestBrowser(true, false));
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));
  ASSERT_TRUE(ExistingWindowSubMenuModel::ShouldShowSubmenu(
      incognito_browser_2->GetProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true)));
}

// Ensure we don't show the menu on a popup window.
IN_PROC_BROWSER_TEST_F(ExistingWindowSubMenuModelTest, ShouldShowSubmenuPopup) {
  // Popup windows aren't counted when determining whether to show the menu.
  Browser* browser_2(CreateTestBrowser(false, true));
  ASSERT_FALSE(
      ExistingWindowSubMenuModel::ShouldShowSubmenu(browser_2->GetProfile()));

  // Add another tabbed window, make sure the menu shows.
  Browser* browser_3(CreateTestBrowser(false, false));
  ASSERT_TRUE(
      ExistingWindowSubMenuModel::ShouldShowSubmenu(browser_3->GetProfile()));
}

// Validate that windows appear in MRU order and with the expected labels.
IN_PROC_BROWSER_TEST_F(ExistingWindowSubMenuModelTest, BuildSubmenuOrder) {
  // Add some browsers.
  ActivateBrowser(browser());
  Browser* browser_2(CreateTestBrowser(false, false));
  Browser* browser_3(CreateTestBrowser(false, false));
  Browser* browser_4(CreateTestBrowser(false, false));

  // Add tabs.
  constexpr char kLongTabTitleExample[] =
      "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Maecenas "
      "porttitor congue massa. Fusce posuere, magna sed pulvinar ultricies,"
      "purus lectus malesuada libero, sit amet commodo magna eros quis urna.";

  AddTabWithTitle(browser(), "Browser 1");
  AddTabWithTitle(browser_2, kLongTabTitleExample);
  AddTabWithTitle(browser_3, "Browser 3 Tab 1");
  AddTabWithTitle(browser_3, "Browser 3 Tab 2");
  AddTabWithTitle(browser_4, "Browser 4 Tab 1");
  AddTabWithTitle(browser_4, "Browser 4 Tab 2");
  AddTabWithTitle(browser_4, kLongTabTitleExample);

  // Create menu from browser 1.
  auto menu1 = ExistingWindowSubMenuModel::Create(
      nullptr, browser()->GetFeatures().tab_menu_model_delegate(),
      browser()->tab_strip_model(), 0);
  ASSERT_EQ(5u, menu1->GetItemCount());
  CheckBrowserTitle(menu1->GetLabelAt(2), kLongTabTitleExample, 3);
  CheckBrowserTitle(menu1->GetLabelAt(3), "Browser 3 Tab 2", 2);
  CheckBrowserTitle(menu1->GetLabelAt(4), kLongTabTitleExample, 1);

  // Create menu from browser 2.
  auto menu2 = ExistingWindowSubMenuModel::Create(
      nullptr, browser_2->GetFeatures().tab_menu_model_delegate(),
      browser_2->tab_strip_model(), 0);
  ASSERT_EQ(5u, menu2->GetItemCount());
  CheckBrowserTitle(menu2->GetLabelAt(2), kLongTabTitleExample, 3);
  CheckBrowserTitle(menu2->GetLabelAt(3), "Browser 3 Tab 2", 2);
  CheckBrowserTitle(menu2->GetLabelAt(4), "Browser 1", 1);

  // Rearrange the MRU and re-test.
  ActivateBrowser(browser_3);
  ActivateBrowser(browser_4);
  ActivateBrowser(browser());
  ActivateBrowser(browser_2);

  auto menu3 = ExistingWindowSubMenuModel::Create(
      nullptr, browser_3->GetFeatures().tab_menu_model_delegate(),
      browser_3->tab_strip_model(), 0);
  ASSERT_EQ(5u, menu3->GetItemCount());
  CheckBrowserTitle(menu3->GetLabelAt(2), kLongTabTitleExample, 1);
  CheckBrowserTitle(menu3->GetLabelAt(3), "Browser 1", 1);
  CheckBrowserTitle(menu3->GetLabelAt(4), kLongTabTitleExample, 3);

  // Clean up.
  CloseBrowserSynchronously(browser_2);
  CloseBrowserSynchronously(browser_3);
  CloseBrowserSynchronously(browser_4);
}

// Ensure that normal browsers and incognito browsers have their own lists.
IN_PROC_BROWSER_TEST_F(ExistingWindowSubMenuModelTest, BuildSubmenuIncognito) {
  // Add some browsers.
  ActivateBrowser(browser());
  Browser* browser_2(CreateTestBrowser(false, false));
  Browser* browser_3(CreateTestBrowser(false, false));
  Browser* incognito_browser_1(CreateTestBrowser(true, false));
  Browser* incognito_browser_2(CreateTestBrowser(true, false));

  AddTabWithTitle(browser(), "Browser 1");
  AddTabWithTitle(browser_2, "Browser 2");
  AddTabWithTitle(browser_3, "Browser 3");
  AddTabWithTitle(incognito_browser_1, "Incognito Browser 1");
  AddTabWithTitle(incognito_browser_2, "Incognito Browser 2");

  const std::u16string kBrowser2ExpectedTitle = u"Browser 2";
  const std::u16string kBrowser3ExpectedTitle = u"Browser 3";
  const std::u16string kIncognitoBrowser2ExpectedTitle = u"Incognito Browser 2";

  // Test that a non-incognito browser only shows non-incognito windows.
  auto menu = ExistingWindowSubMenuModel::Create(
      nullptr, browser()->GetFeatures().tab_menu_model_delegate(),
      browser()->tab_strip_model(), 0);
  ASSERT_EQ(4u, menu->GetItemCount());
  ASSERT_EQ(kBrowser3ExpectedTitle, menu->GetLabelAt(2));
  ASSERT_EQ(kBrowser2ExpectedTitle, menu->GetLabelAt(3));

  // Test that a incognito browser only shows incognito windows.
  auto menu_incognito = ExistingWindowSubMenuModel::Create(
      nullptr, incognito_browser_1->GetFeatures().tab_menu_model_delegate(),
      incognito_browser_1->tab_strip_model(), 0);
  ASSERT_EQ(3u, menu_incognito->GetItemCount());
  ASSERT_EQ(kIncognitoBrowser2ExpectedTitle, menu_incognito->GetLabelAt(2));

  // Clean up.
  CloseBrowserSynchronously(browser_2);
  CloseBrowserSynchronously(browser_3);
  CloseBrowserSynchronously(incognito_browser_1);
  CloseBrowserSynchronously(incognito_browser_2);
}

// Ensure that popups don't appear in the list of existing windows.
IN_PROC_BROWSER_TEST_F(ExistingWindowSubMenuModelTest, BuildSubmenuPopups) {
  // Add some browsers.
  ActivateBrowser(browser());
  Browser* browser_2(CreateTestBrowser(false, false));
  Browser* browser_3(CreateTestBrowser(false, false));
  Browser* popup_browser_1(CreateTestBrowser(false, true));
  Browser* popup_browser_2(CreateTestBrowser(false, true));

  AddTabWithTitle(browser(), "Browser 1");
  AddTabWithTitle(browser_2, "Browser 2");
  AddTabWithTitle(browser_3, "Browser 3");

  const std::u16string kBrowser2ExpectedTitle = u"Browser 2";
  const std::u16string kBrowser3ExpectedTitle = u"Browser 3";

  // Test that popups do not show.
  auto menu = ExistingWindowSubMenuModel::Create(
      nullptr, browser()->GetFeatures().tab_menu_model_delegate(),
      browser()->tab_strip_model(), 0);
  ASSERT_EQ(4u, menu->GetItemCount());
  ASSERT_EQ(kBrowser3ExpectedTitle, menu->GetLabelAt(2));
  ASSERT_EQ(kBrowser2ExpectedTitle, menu->GetLabelAt(3));

  // Clean up.
  CloseBrowserSynchronously(browser_2);
  CloseBrowserSynchronously(browser_3);
  CloseBrowserSynchronously(popup_browser_1);
  CloseBrowserSynchronously(popup_browser_2);
}

}  // namespace
