// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_BUILDER_H_

#include <memory>

#include "chrome/browser/ui/views/frame/system_menu_model_delegate.h"
#include "ui/base/interaction/element_identifier.h"

class BrowserWindowInterface;

namespace ui {
class AcceleratorProvider;
class MenuModel;
class SimpleMenuModel;
}  // namespace ui

// SystemMenuModelBuilder is responsible for building and owning the system menu
// model.
class SystemMenuModelBuilder {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToggleVerticalTabsElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToggleVerticalTabsCollapseElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kToggleVerticalTabsExpandOnHoverElementId);
  SystemMenuModelBuilder(ui::AcceleratorProvider* provider,
                         BrowserWindowInterface* browser);

  SystemMenuModelBuilder(const SystemMenuModelBuilder&) = delete;
  SystemMenuModelBuilder& operator=(const SystemMenuModelBuilder&) = delete;

  ~SystemMenuModelBuilder();

  // Populates the menu.
  void Init();

  // Returns the menu model. SystemMenuModelBuilder owns the returned model.
  ui::MenuModel* menu_model() { return menu_model_.get(); }

 private:
  BrowserWindowInterface* browser() { return menu_delegate_.browser(); }

  // Populates |model| with the appropriate contents.
  void BuildMenu(ui::SimpleMenuModel* model);
  void BuildSystemMenuForBrowserWindow(ui::SimpleMenuModel* model);
  void BuildSystemMenuForAppOrPopupWindow(ui::SimpleMenuModel* model);

  // Add the items to allow the window to visit the desktop of another user.
  void AppendTeleportMenu(ui::SimpleMenuModel* model);

  SystemMenuModelDelegate menu_delegate_;
  std::unique_ptr<ui::MenuModel> menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> zoom_menu_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_BUILDER_H_
