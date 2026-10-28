// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/menus/simple_menu_model.h"

class RenderViewContextMenuBase;

namespace autofill {

class AutofillDriver;

// `AutofillContextMenuManager` is responsible for adding/executing Autofill
// related context menu items. `RenderViewContextMenu` is intended to own and
// control the lifetime of `AutofillContextMenuManager`.
// The options include:
// Fill in Form
class AutofillContextMenuManager : public RenderViewContextMenuObserver {
 public:
  // Represents command id used to denote a row in the context menu. The
  // command ids are created when the items are added to the context menu during
  // it's initialization.
  using CommandId = base::StrongAlias<class CommandIdTag, int>;

  AutofillContextMenuManager(RenderViewContextMenuBase* delegate,
                             ui::SimpleMenuModel* menu_model);
  ~AutofillContextMenuManager() override;
  AutofillContextMenuManager(const AutofillContextMenuManager&) = delete;
  AutofillContextMenuManager& operator=(const AutofillContextMenuManager&) =
      delete;

  // Adds items to the context menu.
  // Note: This doesn't use `RenderViewContextMenuObserver::InitMenu()`, since
  // Autofill context menu entries are conditioned on
  // `ContextMenuContentType::ITEM_GROUP_AUTOFILL`.
  void AppendItems();

  // `RenderViewContextMenuObserver` overrides.
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

  // Setter for `params_` used for testing purposes.
  void set_params_for_testing(content::ContextMenuParams params) {
    params_ = params;
  }

 private:
  // Conditionally adds the Autofill manual fallbacks to the context menu model
  // depending on whether there's data to suggest.
  void MaybeAddAutofillManualFallbackItems();

  // Adds the AtMemory manual fallback item if the feature is enabled. Returns
  // true if the item was added, false otherwise.
  bool MaybeAddAtMemoryItem();

  // Triggers @memory search popup on the field that the context menu was
  // opened on.
  void ExecuteFallbackForAtMemoryCommand(AutofillDriver& driver);

  const raw_ptr<ui::SimpleMenuModel> menu_model_;
  const raw_ptr<RenderViewContextMenuBase> delegate_;
  content::ContextMenuParams params_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
