// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/ui/download/download_bubble_security_view_info.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_navigation_handler.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_client_view.h"

namespace {
using offline_items_collection::ContentId;

constexpr int kAfterParagraphSpacing = 8;

// Whether we should page away from the security view and return to the primary
// view upon a download update.
bool ShouldReturnToPrimaryDialog(const DownloadBubbleSecurityViewInfo& info) {
  return !info.HasSubpage();
}

bool HandleButtonClickWithDefaultClose(
    base::WeakPtr<DownloadBubbleSecurityView> security_view_weak,
    DownloadCommands::Command command,
    bool is_secondary_button) {
  if (!security_view_weak) {
    // Close the dialog.
    return true;
  }
  return security_view_weak->ProcessButtonClick(command, is_secondary_button);
}

}  // namespace

// This class encapsulates a piece of text broken into several paragraphs.
class ParagraphsView : public views::View {
  METADATA_HEADER(ParagraphsView, views::View)

 public:
  ParagraphsView() {
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kUnbounded,
                                 /*adjust_height_for_width=*/true));
    SetProperty(views::kCrossAxisAlignmentKey,
                views::LayoutAlignment::kStretch);
  }
  ~ParagraphsView() override = default;

  void SetText(const std::u16string& text) {
    paragraphs_.clear();
    RemoveAllChildViews();

    for (const auto& paragraph : base::SplitStringUsingSubstr(
             text, u"\n\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      views::StyledLabel* label =
          AddChildView(std::make_unique<views::StyledLabel>());
      label->SetProperty(views::kCrossAxisAlignmentKey,
                         views::LayoutAlignment::kStretch);
      label->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                   views::MaximumFlexSizeRule::kUnbounded,
                                   /*adjust_height_for_width=*/true));
      label->SetText(paragraph);
      paragraphs_.push_back(label);
    }

    Update();
  }

  void SetTextContext(int text_context) {
    if (text_context_ == text_context) {
      return;
    }

    text_context_ = text_context;
    Update();
  }

  void SetDefaultTextStyle(int text_style) {
    if (default_text_style_ == text_style) {
      return;
    }

    default_text_style_ = text_style;
    Update();
  }

  int GetLineHeight() {
    return views::TypographyProvider::Get().GetLineHeight(text_context_,
                                                          default_text_style_);
  }

  void SetAfterParagraph(int spacing) {
    if (after_paragraph_ == spacing) {
      return;
    }

    after_paragraph_ = spacing;
    Update();
  }

  void SizeToFit(int fixed_width) {
    if (fixed_width_ == fixed_width) {
      return;
    }

    fixed_width_ = fixed_width;
    Update();
  }

 private:
  void Update() {
    for (views::StyledLabel* label : paragraphs_) {
      label->SetTextContext(text_context_);
      label->SetDefaultTextStyle(default_text_style_);
      label->SetProperty(views::kMarginsKey,
                         gfx::Insets().set_bottom(after_paragraph_));
      label->SizeToFit(fixed_width_);
      label->PreferredSizeChanged();
    }

    if (!paragraphs_.empty()) {
      paragraphs_.back()->SetProperty(views::kMarginsKey, gfx::Insets());
      PreferredSizeChanged();
    }
  }

  int text_context_ = views::style::CONTEXT_LABEL;
  int default_text_style_ = views::style::STYLE_PRIMARY;
  int after_paragraph_ = 0;
  int fixed_width_ = 0;
  std::vector<raw_ptr<views::StyledLabel, VectorExperimental>> paragraphs_;
};

BEGIN_METADATA(ParagraphsView)
END_METADATA

bool DownloadBubbleSecurityView::IsInitialized() const {
  return info_->content_id().has_value();
}

void DownloadBubbleSecurityView::AddHeader() {
  auto* header = AddChildView(std::make_unique<views::View>());
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);

  back_button_ =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&DownloadBubbleSecurityView::BackButtonPressed,
                              base::Unretained(this)),
          features::IsRoundedIconsEnabled()
              ? vector_icons::kArrowBackIcon
              : vector_icons::kArrowBackChromeRefreshOldIcon,
          GetLayoutConstant(LayoutConstant::kDownloadIconSize)));
  views::InstallCircleHighlightPathGenerator(back_button_);
  back_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_BACK_RECENT_DOWNLOADS));
  back_button_->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStart);

  title_ = header->AddChildView(std::make_unique<views::StyledLabel>());
  title_->SetDefaultTextStyle(views::style::STYLE_PRIMARY);
  title_->SetTextContext(views::style::CONTEXT_DIALOG_TITLE);
  title_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  title_->SetProperty(views::kMarginsKey,
                      gfx::Insets::VH(0, icon_label_spacing));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetDefaultTextStyle(views::style::STYLE_HEADLINE_4);

  auto* close_button =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&DownloadBubbleSecurityView::CloseBubble,
                              base::Unretained(this)),
          features::IsRoundedIconsEnabled()
              ? vector_icons::kCloseIcon
              : vector_icons::kCloseChromeRefreshOldIcon,
          GetLayoutConstant(LayoutConstant::kDownloadIconSize)));
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  InstallCircleHighlightPathGenerator(close_button);
  close_button->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStart);
}

void DownloadBubbleSecurityView::BackButtonPressed() {
  if (IsInitialized()) {
    delegate_->AddSecuritySubpageWarningActionEvent(
        content_id(), DownloadItemWarningData::WarningAction::BACK);
    did_log_action_ = true;
  }
  navigation_handler_->OpenPrimaryDialog();
}

void DownloadBubbleSecurityView::UpdateHeader() {
  title_->SetText(info_->title_text());
  title_->SizeToFit(GetMinimumTitleWidth());
  title_->PreferredSizeChanged();
}

void DownloadBubbleSecurityView::CloseBubble() {
  if (IsInitialized()) {
    delegate_->AddSecuritySubpageWarningActionEvent(
        content_id(), DownloadItemWarningData::WarningAction::CLOSE);
    did_log_action_ = true;
  }
  // CloseDialog will delete the object. Do not access any members below.
  navigation_handler_->CloseDialog(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void DownloadBubbleSecurityView::UpdateIconAndText() {
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      *(info_->icon_model_override()), info_->secondary_color(),
      GetLayoutConstant(LayoutConstant::kDownloadIconSize)));

  paragraphs_->SetText(info_->warning_summary());

  // The label defaults to a single line, which would force the dialog wider;
  // instead give it a width that's the minimum we want it to have. Then the
  // Layout will stretch it back out into any additional space available.
  paragraphs_->SizeToFit(GetMinimumLabelWidth());
}

void DownloadBubbleSecurityView::AddIconAndContents() {
  const int side_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  auto* icon_text_row = AddChildView(std::make_unique<views::View>());
  icon_text_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  icon_text_row->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(side_margin, 0));

  icon_ = icon_text_row->AddChildView(std::make_unique<views::ImageView>());
  icon_->SetProperty(views::kMarginsKey, GetLayoutInsets(DOWNLOAD_ICON));
  const int icon_size = GetLayoutConstant(LayoutConstant::kDownloadIconSize);
  icon_->SetImageSize({icon_size, icon_size});

  auto* wrapper = icon_text_row->AddChildView(std::make_unique<views::View>());
  wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  wrapper->SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(0, icon_label_spacing, 0, 0));
  wrapper->SetProperty(views::kCrossAxisAlignmentKey,
                       views::LayoutAlignment::kStretch);
  wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));

  paragraphs_ = wrapper->AddChildView(std::make_unique<ParagraphsView>());
  paragraphs_->SetProperty(views::kCrossAxisAlignmentKey,
                           views::LayoutAlignment::kStretch);
  paragraphs_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  paragraphs_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));
  paragraphs_->SetAfterParagraph(kAfterParagraphSpacing);
  paragraphs_->SetDefaultTextStyle(views::style::STYLE_BODY_3);
  // Align the centers of icon and the first line of label.
  paragraphs_->SetProperty(
      views::kMarginsKey,
      gfx::Insets().set_top(icon_size / 2 +
                            GetLayoutInsets(DOWNLOAD_ICON).top() -
                            paragraphs_->GetLineHeight() / 2));
}

bool DownloadBubbleSecurityView::ProcessButtonClick(
    DownloadCommands::Command command,
    bool is_secondary_button) {
  if (!IsInitialized()) {
    return true;
  }
  if (!navigation_handler_) {
    // If the navigation handler has gone away, close the dialog.
    return true;
  }

  // Process the command first, since this may become uninitialized once the
  // navigation occurs.
  delegate_->ProcessSecuritySubpageButtonPress(content_id(), command);
  return true;
}

void DownloadBubbleSecurityView::MaybeLogDismiss() {
  if (did_log_action_ || !IsInitialized()) {
    return;
  }
  delegate_->AddSecuritySubpageWarningActionEvent(
      content_id(), DownloadItemWarningData::WarningAction::DISMISS);
  did_log_action_ = true;
}

const offline_items_collection::ContentId&
DownloadBubbleSecurityView::content_id() const {
  CHECK(info_->content_id().has_value());
  return info_->content_id().value();
}

void DownloadBubbleSecurityView::UpdateButton(
    DownloadBubbleSecurityViewInfo::SubpageButton button_info,
    bool is_secondary_button) {
  ui::mojom::DialogButton button_type = is_secondary_button
                                            ? ui::mojom::DialogButton::kCancel
                                            : ui::mojom::DialogButton::kOk;

  base::RepeatingCallback<bool()> callback(base::BindRepeating(
      &HandleButtonClickWithDefaultClose, weak_factory_.GetWeakPtr(),
      button_info.command, is_secondary_button));

  bubble_delegate_->SetButtonEnabled(button_type, !occluded_);

  if (button_type == ui::mojom::DialogButton::kCancel) {
    bubble_delegate_->SetCancelCallbackWithClose(callback);
    views::LabelButton* button = bubble_delegate_->GetCancelButton();
    if (button_info.text_color) {
      button->SetEnabledTextColors(*button_info.text_color);
    }
  } else {
    bubble_delegate_->SetAcceptCallbackWithClose(callback);
  }

  bubble_delegate_->SetButtonLabel(button_type, button_info.label);
  if (button_info.is_prominent) {
    bubble_delegate_->SetDefaultButton(static_cast<int>(button_type));
  }
}

void DownloadBubbleSecurityView::UpdateButtons() {
  bubble_delegate_->SetButtons(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate_->SetDefaultButton(
      static_cast<int>(ui::mojom::DialogButton::kNone));

  if (info_->has_primary_button()) {
    bubble_delegate_->SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kOk));
    UpdateButton(info_->primary_button(), /*is_secondary_button=*/false);
  }

  if (info_->has_secondary_button()) {
    bubble_delegate_->SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kCancel) |
        static_cast<int>(ui::mojom::DialogButton::kOk));
    UpdateButton(info_->secondary_button(), /*is_secondary_button=*/true);
  }
  // After we have updated the buttons, set the minimum width to avoid the rest
  // of the contents stretching out the dialog unnecessarily.
  bubble_delegate_->set_fixed_width(GetMinimumBubbleWidth());
  PreferredSizeChanged();
}

void DownloadBubbleSecurityView::ClearWideFields() {
  bubble_delegate_->set_fixed_width(0);
  bubble_delegate_->SetButtonLabel(ui::mojom::DialogButton::kCancel,
                                   std::u16string());
  bubble_delegate_->SetButtonLabel(ui::mojom::DialogButton::kOk,
                                   std::u16string());
  paragraphs_->SetText(std::u16string());
  // Setting an extremely low value here will force the labels to break text
  // into a large number of labels and lay them out, which is wasteful. We set a
  // conservative value of 200 here, which is small enough to ensure the bubble
  // can shrink to minimum width, but not so small as to cause performance
  // problems.
  CHECK_GE(views::LayoutProvider::Get()->GetDistanceMetric(
               views::DistanceMetric::DISTANCE_BUBBLE_PREFERRED_WIDTH),
           200);
  paragraphs_->SizeToFit(200);

  title_->SetText(std::u16string());
}

void DownloadBubbleSecurityView::UpdateViews() {
  CHECK(IsInitialized());
  CHECK(info_->HasSubpage());

  // Our multiline labels need to know the width of the bubble in order to size
  // themselves appropriately (see `GetMinimumLabelWidth`). This means that we
  // must reset fields that increase the width of the bubble before update. This
  // avoids problems where, e.g., both the buttons and the text used to be wide
  // but aren't anymore. A single round of updates could still have a wide
  // bubble.
  ClearWideFields();
  UpdateButtons();
  UpdateHeader();
  UpdateIconAndText();
}

void DownloadBubbleSecurityView::UpdateAccessibilityTextAndFocus() {
  if (!IsInitialized()) {
    return;
  }
  // Announce that the subpage was opened to inform the user about the changes
  // in the UI.
  GetViewAccessibility().AnnounceText(info_->warning_summary());

  // Focus the back button by default to ensure that focus is set when new
  // content is displayed.
  back_button_->RequestFocus();
}

DownloadBubbleSecurityView::DownloadBubbleSecurityView(
    Delegate* delegate,
    const DownloadBubbleSecurityViewInfo& info,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    views::BubbleDialogDelegate* bubble_delegate)
    : delegate_(delegate),
      info_(info),
      navigation_handler_(std::move(navigation_handler)),
      bubble_delegate_(bubble_delegate) {
  CHECK(delegate_);
  info_->AddObserver(this);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  SetProperty(views::kMarginsKey, GetLayoutInsets(DOWNLOAD_ROW));

  AddHeader();
  AddIconAndContents();
}

DownloadBubbleSecurityView::~DownloadBubbleSecurityView() = default;

void DownloadBubbleSecurityView::AddedToWidget() {
  views::Widget* widget = GetWidget();
  pip_occlusion_observation_.Observe(widget);
}

void DownloadBubbleSecurityView::OnOcclusionStateChanged(bool occluded) {
  if (occluded_ == occluded) {
    return;
  }

  // If transitioning from occluded to un-occluded, restart the input protection
  // timer to prevent clickjacking/unintended clicks.
  if (occluded_ && !occluded && bubble_delegate_) {
    bubble_delegate_->TriggerInputProtection();
  }
  occluded_ = occluded;
  UpdateButtons();
}

int DownloadBubbleSecurityView::GetMinimumBubbleWidth() const {
  return ChromeLayoutProvider::Get()->GetSnappedDialogWidth(
      bubble_delegate_->GetDialogClientView()->GetMinimumSize().width());
}

int DownloadBubbleSecurityView::GetMinimumTitleWidth() const {
  // The title's width is similar to the subpage summary's width except it is
  // narrower to accommodate the close button.
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  return GetMinimumLabelWidth() -
         GetLayoutConstant(LayoutConstant::kDownloadIconSize) -
         icon_label_spacing;
}

int DownloadBubbleSecurityView::GetMinimumLabelWidth() const {
  const int side_margin = GetLayoutInsets(DOWNLOAD_ROW).width();
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  return GetMinimumBubbleWidth() - side_margin -
         GetLayoutConstant(LayoutConstant::kDownloadIconSize) -
         GetLayoutInsets(DOWNLOAD_ICON).width() - icon_label_spacing;
}

void DownloadBubbleSecurityView::OnInfoChanged() {
  // If this represents a "terminal" state of a deep scan, or if the download
  // is otherwise no longer dangerous, we return to the primary dialog. Note
  // that we want this behavior even if this is a different download, e.g.
  // user clicks on a different download via entry point external to the
  // download bubble.
  if (ShouldReturnToPrimaryDialog(info_.get())) {
    navigation_handler_->OpenPrimaryDialog();
    // No need to update views here because we're resetting and returning to
    // the primary dialog anyway.
    return;
  }
  UpdateViews();
  UpdateAccessibilityTextAndFocus();
}

void DownloadBubbleSecurityView::OnContentIdChanged() {
  // Reset this to false because now this represents a different instance of
  // the security dialog. This should not be reset anywhere else. We only want
  // to consider it a different instance of the dialog (and potentially log a
  // new action) when the action is performed by a user, not when the browser
  // changes the danger type (when a scan is finished, for instance).
  did_log_action_ = false;
}

BEGIN_METADATA(DownloadBubbleSecurityView)
END_METADATA
