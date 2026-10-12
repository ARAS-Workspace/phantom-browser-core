// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_VIEW_H_

#include <map>
#include <string>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/views/media_item_ui_device_selector.h"
#include "components/media_message_center/notification_theme.h"
#include "media/audio/audio_device_description.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace {
inline constexpr char kAudioDevicesCountHistogramName[] =
    "Media.GlobalMediaControls.NumberOfAvailableAudioDevices";
inline constexpr char kDeviceSelectorAvailableHistogramName[] =
    "Media.GlobalMediaControls.DeviceSelectorAvailable";
inline constexpr char kDeviceSelectorOpenedHistogramName[] =
    "Media.GlobalMediaControls.DeviceSelectorOpened";
}  // anonymous namespace

namespace global_media_controls {
class MediaItemUIView;
}  // namespace global_media_controls

class MediaItemUIDeviceSelectorDelegate;

// A device selector view for Chrome OS only.
class MediaItemUIDeviceSelectorView
    : public global_media_controls::MediaItemUIDeviceSelector,
      public IconLabelBubbleView::Delegate {
  METADATA_HEADER(MediaItemUIDeviceSelectorView,
                  global_media_controls::MediaItemUIDeviceSelector)
 public:
  MediaItemUIDeviceSelectorView(
      const std::string& item_id,
      MediaItemUIDeviceSelectorDelegate* delegate,
      bool has_audio_output,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point,
      media_message_center::MediaColorTheme media_color_theme,
      bool show_devices = false);
  ~MediaItemUIDeviceSelectorView() override;

  // Called when audio output devices are discovered.
  void UpdateAvailableAudioDevices(
      const media::AudioDeviceDescriptions& device_descriptions);

  // global_media_controls::MediaItemUIDeviceSelector:
  void SetMediaItemUIView(
      global_media_controls::MediaItemUIView* view) override;
  void OnColorsChanged(SkColor foreground_color,
                       SkColor background_color) override;
  void UpdateCurrentAudioDevice(const std::string& current_device_id) override;
  void ShowDevices() override;
  void HideDevices() override;
  bool IsDeviceSelectorExpanded() override;

  // Called when the audio device switching has become enabled or disabled.
  void UpdateIsAudioDeviceSwitchingEnabled(bool enabled);

  // IconLabelBubbleView::Delegate
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override;
  SkColor GetIconLabelBubbleBackgroundColor() const override;

  // views::View
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

  std::string GetEntryLabelForTesting(views::View* entry_view);
  bool GetEntryIsHighlightedForTesting(views::View* entry_view);
  bool GetDeviceEntryViewVisibilityForTesting();

 private:
  friend class MediaItemUIDeviceSelectorViewTest;
  FRIEND_TEST_ALL_PREFIXES(MediaItemUIDeviceSelectorViewTest,
                           AudioDevicesCountHistogramRecorded);
  FRIEND_TEST_ALL_PREFIXES(MediaItemUIDeviceSelectorViewTest,
                           DeviceSelectorOpenedHistogramRecorded);

  void UpdateVisibility();
  bool ShouldBeVisible() const;
  void ShowOrHideDeviceList();
  void RemoveDevicesOfType(DeviceEntryUIType type);
  DeviceEntryUI* GetDeviceEntryUI(views::View* view) const;
  void RegisterAudioDeviceCallbacks();

  bool has_view_been_shown_ = false;
  bool have_devices_been_shown_ = false;

  bool is_expanded_ = false;
  bool is_audio_device_switching_enabled_ = false;
  const std::string item_id_;
  const raw_ptr<MediaItemUIDeviceSelectorDelegate, DanglingUntriaged> delegate_;
  std::string current_device_id_ =
      media::AudioDeviceDescription::kDefaultDeviceId;
  SkColor foreground_color_ = global_media_controls::kDefaultForegroundColor;
  SkColor background_color_ = global_media_controls::kDefaultBackgroundColor;
  global_media_controls::GlobalMediaControlsEntryPoint const entry_point_;
  media_message_center::MediaColorTheme media_color_theme_;

  // Child views
  raw_ptr<AudioDeviceEntryView, DanglingUntriaged>
      current_audio_device_entry_view_ = nullptr;
  raw_ptr<views::View> device_entry_views_container_ = nullptr;
  raw_ptr<views::View> permission_error_view_container_ = nullptr;

  base::CallbackListSubscription audio_device_subscription_;
  base::CallbackListSubscription is_device_switching_enabled_subscription_;

  raw_ptr<global_media_controls::MediaItemUIView> media_item_ui_ = nullptr;

  // Each button has a unique tag, which is used to look up DeviceEntryUI* in
  // |device_entry_ui_map_|.
  int next_tag_ = 0;
  std::map<int, raw_ptr<DeviceEntryUI, CtnExperimental>> device_entry_ui_map_;

  base::WeakPtrFactory<MediaItemUIDeviceSelectorView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_VIEW_H_
