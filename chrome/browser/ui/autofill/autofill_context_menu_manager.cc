// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include "base/containers/fixed_flat_set.h"
#include "base/notreached.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/common/aliases.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "ui/menus/simple_menu_model.h"

namespace autofill {

namespace {

bool ShouldShowAutofillContextMenu(const content::ContextMenuParams& params) {
  if (params.is_content_editable_for_autofill) {
    return true;
  }
  if (!params.form_control_type) {
    return false;
  }
  // Return true (only) on text fields.
  //
  // Note that this switch is over `blink::mojom::FormControlType`, not
  // `autofill::FormControlType`. Standard form controls are handled by this
  // switch, while `contenteditable` elements are handled by the
  // `is_content_editable_for_autofill` check above.
  //
  // TODO(crbug.com/40285492): Unify with functions from form_autofill_util.cc.
  switch (*params.form_control_type) {
    case blink::mojom::FormControlType::kInputEmail:
    case blink::mojom::FormControlType::kInputMonth:
    case blink::mojom::FormControlType::kInputNumber:
    case blink::mojom::FormControlType::kInputPassword:
    case blink::mojom::FormControlType::kInputSearch:
    case blink::mojom::FormControlType::kInputTelephone:
    case blink::mojom::FormControlType::kInputText:
    case blink::mojom::FormControlType::kInputUrl:
    case blink::mojom::FormControlType::kTextArea:
      return true;
    case blink::mojom::FormControlType::kButtonButton:
    case blink::mojom::FormControlType::kButtonSubmit:
    case blink::mojom::FormControlType::kButtonReset:
    case blink::mojom::FormControlType::kButtonPopover:
    case blink::mojom::FormControlType::kFieldset:
    case blink::mojom::FormControlType::kInputButton:
    case blink::mojom::FormControlType::kInputCheckbox:
    case blink::mojom::FormControlType::kInputColor:
    case blink::mojom::FormControlType::kInputDate:
    case blink::mojom::FormControlType::kInputDatetimeLocal:
    case blink::mojom::FormControlType::kInputFile:
    case blink::mojom::FormControlType::kInputHidden:
    case blink::mojom::FormControlType::kInputImage:
    case blink::mojom::FormControlType::kInputRadio:
    case blink::mojom::FormControlType::kInputRange:
    case blink::mojom::FormControlType::kInputReset:
    case blink::mojom::FormControlType::kInputSubmit:
    case blink::mojom::FormControlType::kInputTime:
    case blink::mojom::FormControlType::kInputWeek:
    case blink::mojom::FormControlType::kOutput:
    case blink::mojom::FormControlType::kSelectOne:
    case blink::mojom::FormControlType::kSelectMultiple:
      return false;
  }
  NOTREACHED();
}

// Returns true if the given id is one generated for autofill context menu.
bool IsAutofillCustomCommandId(
    AutofillContextMenuManager::CommandId command_id) {
  static constexpr auto kAutofillCommands = base::MakeFixedFlatSet<int>({
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY,
  });
  return kAutofillCommands.contains(command_id.value());
}

}  // namespace

AutofillContextMenuManager::AutofillContextMenuManager(
    RenderViewContextMenuBase* delegate,
    ui::SimpleMenuModel* menu_model)
    : menu_model_(menu_model), delegate_(delegate) {
  DCHECK(delegate_);
  params_ = delegate_->params();
}

AutofillContextMenuManager::~AutofillContextMenuManager() = default;

void AutofillContextMenuManager::AppendItems() {
  if (params_.is_content_editable_for_autofill) {
    if (MaybeAddAtMemoryItem()) {
      menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    }
    return;
  }

  MaybeAddAutofillManualFallbackItems();
}

bool AutofillContextMenuManager::IsCommandIdSupported(int command_id) {
  return IsAutofillCustomCommandId(CommandId(command_id));
}

bool AutofillContextMenuManager::IsCommandIdEnabled(int command_id) {
  return true;
}

void AutofillContextMenuManager::ExecuteCommand(int command_id) {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh) {
    return;
  }
  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!autofill_driver) {
    return;
  }
  CHECK(IsAutofillCustomCommandId(CommandId(command_id)));

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY) {
    ExecuteFallbackForAtMemoryCommand(*autofill_driver);
    return;
  }
}

bool AutofillContextMenuManager::MaybeAddAtMemoryItem() {
  content::RenderFrameHost* const rfh = delegate_->GetRenderFrameHost();
  if (!rfh) {
    return false;
  }

  if (params_.form_control_type &&
      params_.form_control_type.value() ==
          blink::mojom::FormControlType::kInputPassword) {
    return false;
  }

  if (!ShouldShowAutofillContextMenu(params_)) {
    return false;
  }

  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!autofill_driver || !autofill_driver->CanShowAutofillUi()) {
    return false;
  }

  if (!MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                autofill_driver->GetAutofillClient(),
                                params_.page_url) ||
      !MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                autofill_driver->GetAutofillClient(),
                                params_.frame_url)) {
    return false;
  }

  menu_model_->AddItemWithStringId(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY,
      IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY);
  return true;
}

void AutofillContextMenuManager::MaybeAddAutofillManualFallbackItems() {
  if (!ShouldShowAutofillContextMenu(params_)) {
    // Autofill entries are only available in input or text area fields
    return;
  }

  if (MaybeAddAtMemoryItem()) {
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }
}

void AutofillContextMenuManager::ExecuteFallbackForAtMemoryCommand(
    AutofillDriver& driver) {
  driver.RendererShouldTriggerSuggestions(
      {driver.GetFrameToken(),
       FieldRendererId(params_.field_renderer_id.value())},
      AutofillSuggestionTriggerSource::kAtMemoryContextMenu);
}

}  // namespace autofill
