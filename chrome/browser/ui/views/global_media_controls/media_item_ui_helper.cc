// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"

#include "ui/color/color_id.h"

media_message_center::MediaColorTheme GetMediaColorTheme() {
  media_message_center::MediaColorTheme theme;
  theme.primary_foreground_color_id = ui::kColorSysOnSurface;
  theme.secondary_foreground_color_id = ui::kColorSysOnSurfaceSubtle;

  // Colors for the play/pause button.
  theme.play_button_foreground_color_id = ui::kColorSysOnPrimary;
  theme.play_button_container_color_id = ui::kColorSysPrimary;
  theme.pause_button_foreground_color_id = ui::kColorSysOnTonalContainer;
  theme.pause_button_container_color_id = ui::kColorSysTonalContainer;

  // Colors for the progress view.
  theme.playing_progress_foreground_color_id = ui::kColorSysPrimary;
  theme.playing_progress_background_color_id =
      ui::kColorSysStateDisabledContainer;
  theme.paused_progress_foreground_color_id =
      ui::kColorSysStateDisabledContainer;
  theme.paused_progress_background_color_id =
      ui::kColorSysStateDisabledContainer;

  theme.background_color_id = ui::kColorSysSurface2;
  theme.device_selector_border_color_id = ui::kColorSysDivider;
  theme.device_selector_foreground_color_id = ui::kColorSysPrimary;
  theme.device_selector_background_color_id = ui::kColorSysSurface5;
  theme.error_foreground_color_id = ui::kColorSysError;
  theme.error_container_color_id = ui::kColorSysErrorContainer;
  theme.focus_ring_color_id = ui::kColorSysStateFocusRing;
  return theme;
}
