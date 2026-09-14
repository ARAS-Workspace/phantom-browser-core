// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/translate_errors.h"
#include "components/translate/core/common/translate_features.h"
#include "components/translate/core/common/translate_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"

namespace {

actions::ActionItem* GetTranslateActionItem(
    actions::ActionItem* root_action_item) {
  actions::ActionItem* translate_action_item =
      actions::ActionManager::Get().FindAction(kActionShowTranslate,
                                               root_action_item);
  CHECK(translate_action_item);
  return translate_action_item;
}

}  // namespace

DEFINE_USER_DATA(TranslateBubbleController);

TranslateBubbleController::TranslateBubbleController(
    BrowserWindowInterface* browser_window,
    actions::ActionItem* root_action_item)
    : browser_window_(browser_window),
      action_item_(GetTranslateActionItem(root_action_item)),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {}

TranslateBubbleController::~TranslateBubbleController() = default;

// static
TranslateBubbleController* TranslateBubbleController::From(
    BrowserWindowInterface* window) {
  return Get(window->GetUnownedUserDataHost());
}

views::Widget* TranslateBubbleController::ShowTranslateBubble(
    content::WebContents* web_contents,
    views::BubbleAnchor anchor,
    std::optional<ui::ElementIdentifier> highlighted_element,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    LocationBarBubbleDelegateView::DisplayReason reason) {
  if (translate_bubble_view_) {
    // When the user reads the advanced setting panel, the bubble should not be
    // changed because they are focusing on the bubble.
    if (translate_bubble_view_->model()->GetViewState() ==
            TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
        translate_bubble_view_->model()->GetViewState() ==
            TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE) {
      return nullptr;
    }
    translate_bubble_view_->SetViewState(step, error_type);
    return nullptr;
  }

  if (step == translate::TRANSLATE_STEP_AFTER_TRANSLATE &&
      reason == LocationBarBubbleDelegateView::AUTOMATIC) {
    return nullptr;
  }

  std::unique_ptr<TranslateBubbleModel> model;
  if (model_factory_callback_) {
    model = model_factory_callback_.Run();
  } else {
    auto ui_delegate = std::make_unique<translate::TranslateUIDelegate>(
        ChromeTranslateClient::GetManagerFromWebContents(web_contents)
            ->GetWeakPtr(),
        source_language, target_language);
    model = std::make_unique<TranslateBubbleModelImpl>(step,
                                                       std::move(ui_delegate));
  }

  auto translate_bubble_view = std::make_unique<TranslateBubbleView>(
      anchor, std::move(model), error_type, web_contents,
      GetOnTranslateBubbleClosedCallback());
  translate_bubble_view_ = translate_bubble_view.get();

  if (highlighted_element) {
    translate_bubble_view_->SetHighlightedElement(*highlighted_element);
  }
  views::Widget* bubble_widget = views::BubbleDialogDelegateView::CreateBubble(
      std::move(translate_bubble_view));

  // TAB UI has the same view throughout. Select the right tab based on |step|
  // upon initialization.
  translate_bubble_view_->SetViewState(step, error_type);
  translate_bubble_view_->ShowForReason(reason);

  translate_bubble_view_->model()->ReportUIChange(true);

  action_item_->SetIsShowingBubble(true);

  // Trigger PDF translate IPH
  if (web_contents &&
      base::FeatureList::IsEnabled(translate::kEnableTranslatePdf)) {
    if (auto* translate_manager =
            ChromeTranslateClient::GetManagerFromWebContents(web_contents)) {
      if (translate_manager->GetLanguageState()->pdf_translatability_status() ==
          translate::LanguageState::PdfTranslatabilityStatus::kTranslatable) {
        if (auto* user_education =
                BrowserUserEducationInterface::From(browser_window_)) {
          user_education->MaybeShowFeaturePromo(
              feature_engagement::kIPHPdfTranslateBubbleFeature);
        }
      }
    }
  }

  return bubble_widget;
}

void TranslateBubbleController::CloseBubble() {
  if (translate_bubble_view_) {
    translate_bubble_view_->CloseBubble();
  }
}

TranslateBubbleView* TranslateBubbleController::GetTranslateBubble() const {
  return translate_bubble_view_;
}

void TranslateBubbleController::SetTranslateBubbleModelFactory(
    base::RepeatingCallback<std::unique_ptr<TranslateBubbleModel>()> callback) {
  model_factory_callback_ = std::move(callback);
}

void TranslateBubbleController::SetAnchorViewForTesting(
    views::View* anchor_view) {
  anchor_view_for_testing_ = anchor_view;
}

base::OnceClosure
TranslateBubbleController::GetOnTranslateBubbleClosedCallback() {
  return base::BindOnce(&TranslateBubbleController::OnTranslateBubbleClosed,
                        weak_ptr_factory_.GetWeakPtr());
}

void TranslateBubbleController::OnTranslateBubbleClosed() {
  translate_bubble_view_ = nullptr;
  action_item_->SetIsShowingBubble(false);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TranslateBubbleController);
