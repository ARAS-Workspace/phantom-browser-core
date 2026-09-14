// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_CONTROLLER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace views {
class View;
}  // namespace views

// Controls the TranslateBubbleView shown for a given browser. This controller
// is responsible for creating and hiding the bubble.
class TranslateBubbleController {
 public:
  DECLARE_USER_DATA(TranslateBubbleController);

  // `root_action_item` is used to retrieve the correct Translate ActionItem.
  TranslateBubbleController(BrowserWindowInterface* browser_window,
                            actions::ActionItem* root_action_item);
  ~TranslateBubbleController();
  TranslateBubbleController(const TranslateBubbleController&) = delete;
  TranslateBubbleController& operator=(const TranslateBubbleController&) =
      delete;

  static TranslateBubbleController* From(BrowserWindowInterface* window);

  // Shows the Full Page Translate bubble. Returns the newly created bubble's
  // Widget or nullptr in cases when the bubble already exists or when the
  // bubble is not created.
  views::Widget* ShowTranslateBubble(
      content::WebContents* web_contents,
      views::BubbleAnchor anchor,
      std::optional<ui::ElementIdentifier> highlighted_element,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors error_type,
      LocationBarBubbleDelegateView::DisplayReason reason);

  // Closes the current Full Page Translate bubble, if it exists.
  void CloseBubble();

  // Returns the currently shown Full Page Translate bubble view. Returns
  // nullptr if the bubble is not currently shown.
  TranslateBubbleView* GetTranslateBubble() const;

  void SetTranslateBubbleModelFactory(
      base::RepeatingCallback<std::unique_ptr<TranslateBubbleModel>()>
          callback);

  void SetAnchorViewForTesting(views::View* anchor_view);

  base::OnceClosure GetOnTranslateBubbleClosedCallback();

 private:
  // Weak reference for the Translate bubble view. This will be nullptr if no
  // bubble is currently shown.
  raw_ptr<TranslateBubbleView> translate_bubble_view_ = nullptr;

  // Factory used to construct the model for the Translate bubble. If the
  // factory is null, the standard implementation - TranslateBubbleModelImpl -
  // is used.
  base::RepeatingCallback<std::unique_ptr<TranslateBubbleModel>()>
      model_factory_callback_;

  // Handler for when the Translate bubble is closed.
  void OnTranslateBubbleClosed();

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  const raw_ptr<BrowserWindowInterface> browser_window_;

  // The action item associated with showing a Translate UI.
  // The bubbles use this to appropriately configure its "IsBubbleShowing"
  // property.
  const raw_ptr<actions::ActionItem> action_item_;

  ui::ScopedUnownedUserData<TranslateBubbleController>
      scoped_unowned_user_data_;

  raw_ptr<views::View> anchor_view_for_testing_ = nullptr;

  base::WeakPtrFactory<TranslateBubbleController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_CONTROLLER_H_
