// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/menus/cocoa/text_services_context_menu.h"

#include <string_view>
#include <utility>

#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// Returns the TextDirection associated associated with the given BiDi
// |command_id|.
base::i18n::TextDirection GetTextDirectionFromCommandId(int command_id) {
  switch (command_id) {
    case ui::TextServicesContextMenu::kWritingDirectionDefault:
      return base::i18n::UNKNOWN_DIRECTION;
    case ui::TextServicesContextMenu::kWritingDirectionLtr:
      return base::i18n::LEFT_TO_RIGHT;
    case ui::TextServicesContextMenu::kWritingDirectionRtl:
      return base::i18n::RIGHT_TO_LEFT;
    default:
      NOTREACHED();
  }
}

}  // namespace

namespace ui {

TextServicesContextMenu::TextServicesContextMenu(Delegate* delegate)
    : bidi_submenu_model_(this),
      delegate_(delegate) {
  DCHECK(delegate);

  bidi_submenu_model_.AddCheckItemWithStringId(
      kWritingDirectionDefault, IDS_CONTENT_CONTEXT_WRITING_DIRECTION_DEFAULT);
  bidi_submenu_model_.AddCheckItemWithStringId(
      kWritingDirectionLtr, IDS_CONTENT_CONTEXT_WRITING_DIRECTION_LTR);
  bidi_submenu_model_.AddCheckItemWithStringId(
      kWritingDirectionRtl, IDS_CONTENT_CONTEXT_WRITING_DIRECTION_RTL);
}

void TextServicesContextMenu::AppendEditableItems(SimpleMenuModel* model) {
  // MacOS provides a contextual menu to set writing direction for BiDi
  // languages. This functionality is exposed as a keyboard shortcut on
  // Windows and Linux.
  model->AddSubMenuWithStringId(kWritingDirectionMenu,
                                IDS_CONTENT_CONTEXT_WRITING_DIRECTION_MENU,
                                &bidi_submenu_model_);
}

bool TextServicesContextMenu::SupportsCommand(int command_id) const {
  switch (command_id) {
    case kWritingDirectionMenu:
    case kWritingDirectionDefault:
    case kWritingDirectionLtr:
    case kWritingDirectionRtl:
      return true;
  }

  return false;
}

bool TextServicesContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case kWritingDirectionDefault:
    case kWritingDirectionLtr:
    case kWritingDirectionRtl:
      return delegate_->IsTextDirectionChecked(
          GetTextDirectionFromCommandId(command_id));
  }

  NOTREACHED();
}

bool TextServicesContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case kWritingDirectionMenu:
      return true;
    case kWritingDirectionDefault:
    case kWritingDirectionLtr:
    case kWritingDirectionRtl:
      return delegate_->IsTextDirectionEnabled(
          GetTextDirectionFromCommandId(command_id));
  }

  NOTREACHED();
}

void TextServicesContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case kWritingDirectionDefault:
    case kWritingDirectionLtr:
    case kWritingDirectionRtl:
      delegate_->UpdateTextDirection(GetTextDirectionFromCommandId(command_id));
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace ui
