// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view_observer.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "components/media_router/browser/media_router.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "net/base/url_util.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

using global_media_controls::GlobalMediaControlsEntryPoint;
using media_session::mojom::MediaSessionAction;

namespace {}  // namespace

// static
MediaDialogView* MediaDialogView::instance_ = nullptr;

// static
bool MediaDialogView::has_been_opened_ = false;

// static
views::Widget* MediaDialogView::ShowDialogFromToolbar(
    views::BubbleAnchor anchor,
    MediaNotificationService* service,
    Profile* profile) {
  return ShowDialog(
      anchor, views::BubbleBorder::TOP_RIGHT, service, profile, nullptr,
      global_media_controls::GlobalMediaControlsEntryPoint::kToolbarIcon);
}

// static
views::Widget* MediaDialogView::ShowDialogCentered(
    const gfx::Rect& bounds,
    MediaNotificationService* service,
    Profile* profile,
    content::WebContents* contents,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point) {
  auto* widget =
      ShowDialog(views::BubbleAnchor(), views::BubbleBorder::TOP_CENTER,
                 service, profile, contents, entry_point);
  instance_->SetAnchorRect(bounds);
  return widget;
}

// static
views::Widget* MediaDialogView::ShowDialog(
    views::BubbleAnchor anchor,
    views::BubbleBorder::Arrow anchor_position,
    MediaNotificationService* service,
    Profile* profile,
    content::WebContents* contents,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point) {
  DCHECK(service);
  // Hide the previous instance if it exists, since there can only be one dialog
  // instance at a time.
  HideDialog();
  instance_ = new MediaDialogView(anchor, anchor_position, service, profile,
                                  contents, entry_point);
  if (!anchor) {
    instance_->set_has_parent(false);
  }

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();

  base::UmaHistogramBoolean("Media.GlobalMediaControls.RepeatUsage",
                            has_been_opened_);
  base::UmaHistogramEnumeration("Media.GlobalMediaControls.EntryPoint",
                                entry_point);
  has_been_opened_ = true;

  return widget;
}

// static
void MediaDialogView::HideDialog() {
  if (IsShowing()) {
    instance_->service_->media_item_manager()->SetDialogDelegate(nullptr);
    instance_->GetWidget()->Close();
  }

  // Set |instance_| to nullptr so that |IsShowing()| returns false immediately.
  // We also set to nullptr in |WindowClosing()| (which happens asynchronously),
  // since |HideDialog()| is not always called.
  instance_ = nullptr;
}

// static
bool MediaDialogView::IsShowing() {
  return instance_ != nullptr;
}

global_media_controls::MediaItemUI* MediaDialogView::ShowMediaItem(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  global_media_controls::MediaItemUI* view_ptr;

  auto view = BuildMediaItemUIUpdatedView(id, item);
  view_ptr = view.get();
  items_[id] = view.get();
  active_sessions_view_->ShowUpdatedItem(id, std::move(view));

  view_ptr->AddObserver(this);
  UpdateBubbleSize();
  observers_.Notify(&MediaDialogViewObserver::OnMediaSessionShown);
  return view_ptr;
}

void MediaDialogView::HideMediaItem(const std::string& id) {
  active_sessions_view_->HideUpdatedItem(id);

  if (active_sessions_view_->empty()) {
    HideDialog();
  } else {
    UpdateBubbleSize();
  }

  observers_.Notify(&MediaDialogViewObserver::OnMediaSessionHidden);
}

void MediaDialogView::RefreshMediaItem(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  if (items_.find(id) == items_.end()) {
    return;
  }
  bool show_devices =
      entry_point_ == GlobalMediaControlsEntryPoint::kPresentation;

  items_[id]->UpdateFooterView(
      BuildFooter(id, item, profile_, media_color_theme_));
  items_[id]->UpdateDeviceSelectorView(
      BuildDeviceSelector(id, item, service_, service_, profile_, entry_point_,
                          media_color_theme_, show_devices));

  UpdateBubbleSize();
}

void MediaDialogView::HideMediaDialog() {
  HideDialog();
}

void MediaDialogView::Focus() {
  RequestFocus();
}

void MediaDialogView::AddedToWidget() {
  int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  views::BubbleFrameView* frame = GetBubbleFrameView();
  if (frame) {
    frame->SetRoundedCorners(gfx::RoundedCornersF(corner_radius));
  }
  if (entry_point_ ==
      global_media_controls::GlobalMediaControlsEntryPoint::kPresentation) {
    service_->SetDialogDelegateForWebContents(
        this, web_contents_for_presentation_request_);
  } else {
    service_->media_item_manager()->SetDialogDelegate(this);
  }
}

gfx::Size MediaDialogView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // If we have active sessions, then fit to them.
  if (!active_sessions_view_->empty()) {
    return views::BubbleDialogDelegateView::CalculatePreferredSize(
        available_size);
  }
  // Otherwise, use a standard size for bubble dialogs.
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, 1);
}

void MediaDialogView::UpdateBubbleSize() {
  SizeToContents();
}



void MediaDialogView::OnMediaItemUISizeChanged() {
  UpdateBubbleSize();
}

void MediaDialogView::OnMediaItemUIMetadataChanged() {
  observers_.Notify(&MediaDialogViewObserver::OnMediaSessionMetadataUpdated);
}

void MediaDialogView::OnMediaItemUIActionsChanged() {
  observers_.Notify(&MediaDialogViewObserver::OnMediaSessionActionsChanged);
}

void MediaDialogView::OnMediaItemUIDestroyed(const std::string& id) {
  items_.erase(id);
}

void MediaDialogView::AddObserver(MediaDialogViewObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaDialogView::RemoveObserver(MediaDialogViewObserver* observer) {
  observers_.RemoveObserver(observer);
}


const std::map<
    const std::string,
    raw_ptr<global_media_controls::MediaItemUIUpdatedView, CtnExperimental>>&
MediaDialogView::GetItemsForTesting() const {
  return active_sessions_view_->updated_items_for_testing();  // IN-TEST
}

const global_media_controls::MediaItemUIListView*
MediaDialogView::GetListViewForTesting() const {
  return active_sessions_view_;
}

MediaDialogView::MediaDialogView(
    views::BubbleAnchor anchor,
    views::BubbleBorder::Arrow anchor_position,
    MediaNotificationService* service,
    Profile* profile,
    content::WebContents* contents,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point)
    : BubbleDialogDelegateView(anchor, anchor_position),
      service_(service),
      profile_(profile->GetOriginalProfile()),
      active_sessions_view_(AddChildView(
          std::make_unique<global_media_controls::MediaItemUIListView>())),
      web_contents_for_presentation_request_(contents),
      entry_point_(entry_point) {
  SetProperty(views::kElementIdentifierKey, kToolbarMediaBubbleElementId);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetAccessibleTitle(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_DIALOG_NAME));
  CHECK(service_);


  media_color_theme_ = GetMediaColorTheme();
}

MediaDialogView::~MediaDialogView() {
  for (auto& item_pair : items_) {
    item_pair.second->RemoveObserver(this);
  }
}

void MediaDialogView::Init() {
  // Remove margins.
  set_margins(gfx::Insets());
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void MediaDialogView::WindowClosing() {
  if (instance_ == this) {
    instance_ = nullptr;
    service_->media_item_manager()->SetDialogDelegate(nullptr);
  }
}











std::unique_ptr<global_media_controls::MediaItemUIUpdatedView>
MediaDialogView::BuildMediaItemUIUpdatedView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  bool show_devices =
      entry_point_ == GlobalMediaControlsEntryPoint::kPresentation;
  return std::make_unique<global_media_controls::MediaItemUIUpdatedView>(
      id, item, media_color_theme_,
      BuildDeviceSelector(id, item, service_, service_, profile_, entry_point_,
                          media_color_theme_, show_devices),
      BuildFooter(id, item, profile_, media_color_theme_));
}

BEGIN_METADATA(MediaDialogView)
END_METADATA
