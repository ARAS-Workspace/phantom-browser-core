// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#endif

// A TabMenuModelDelegate that simulates no other browser windows being open.
class TabMenuModelTestDelegate : public TabMenuModelDelegate {
 public:
  std::vector<BrowserWindowInterface*> GetOtherBrowserWindows(
      bool is_app) override {
    return {};
  }
  tab_groups::TabGroupSyncService* GetTabGroupSyncService() override {
    return nullptr;
  }
};

class TabMenuModelTest : public MenuModelTest, public ::testing::Test {
 public:
  TabMenuModelTest() = default;
  ~TabMenuModelTest() override = default;

  Profile* profile() { return &profile_; }

  TabMenuModelDelegate& menu_model_delegate() { return menu_model_delegate_; }

 private:
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TabMenuModelTestDelegate menu_model_delegate_;
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(TabMenuModelTest, ExtensionItems) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kExtensionTabContextMenu);

  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());

  // Initialize MenuManager for the TestingProfile.
  extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
        return std::make_unique<extensions::MenuManager>(context, nullptr);
      }));

  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      /*foreground=*/true);

  // Create a mock extension.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test Extension").Build();
  ASSERT_TRUE(extension);
  extensions::ExtensionRegistry::Get(profile())->AddEnabled(extension);

  // Create a baseline model to capture the number of standard items.
  TabMenuModel baseline_model(&delegate_, &menu_model_delegate(),
                              &tab_strip_model, 0);
  size_t baseline_count = baseline_model.GetItemCount();

  // Add a context menu item with 'tab' layout context to MenuManager.
  extensions::MenuManager* manager = extensions::MenuManager::Get(profile());
  ASSERT_TRUE(manager);
  extensions::MenuItem::Id id(
      profile()->IsOffTheRecord(),
      extensions::MenuItem::ExtensionKey(extension->id()));
  auto item = std::make_unique<extensions::MenuItem>(
      id, "Test Extension Item", /*checked=*/false, /*visible=*/true,
      /*enabled=*/true, extensions::MenuItem::NORMAL,
      extensions::MenuItem::ContextList(extensions::MenuItem::TAB));

  // Use correct AddContextItem method.
  manager->AddContextItem(extension.get(), std::move(item));

  TabMenuModel model(&delegate_, &menu_model_delegate(), &tab_strip_model, 0);

  // Verify that the menu model successfully populated items (standard +
  // extension).
  size_t count = model.GetItemCount();
  EXPECT_EQ(count, baseline_count + 2);  // Adds 1 separator and 1 item.

  // Find the index of the extension item by searching for an extension custom
  // command ID.
  size_t ext_index = count;
  for (size_t i = 0; i < count; ++i) {
    if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
            model.GetCommandIdAt(i))) {
      ext_index = i;
      break;
    }
  }
  ASSERT_LT(ext_index, count);

  // Cast to base class to access private overrides
  ui::MenuModel* menu_base = &model;
  EXPECT_TRUE(menu_base->IsVisibleAt(ext_index));
  EXPECT_TRUE(menu_base->IsEnabledAt(ext_index));
  EXPECT_FALSE(menu_base->IsItemCheckedAt(ext_index));
}
#endif
