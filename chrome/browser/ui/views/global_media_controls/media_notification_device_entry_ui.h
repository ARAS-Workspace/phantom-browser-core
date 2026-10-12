// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_ENTRY_UI_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_ENTRY_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"

enum class DeviceEntryUIType {
  kAudio = 0,
  kMaxValue = kAudio,
};

class DeviceEntryUI {
 public:
  DeviceEntryUI(const std::string& raw_device_id,
                const std::string& device_name,
                const gfx::VectorIcon& icon,
                const std::string& subtext = "");

  DeviceEntryUI(const DeviceEntryUI&) = delete;
  DeviceEntryUI& operator=(const DeviceEntryUI&) = delete;
  virtual ~DeviceEntryUI() = default;

  const gfx::VectorIcon& icon() const { return *icon_; }
  const std::string& raw_device_id() const { return raw_device_id_; }
  const std::string& device_name() const { return device_name_; }

  virtual void OnColorsChanged(SkColor foreground_color,
                               SkColor background_color) = 0;
  virtual DeviceEntryUIType GetType() const = 0;

  bool GetEntryIsHighlightedForTesting() const { return is_highlighted_; }

 protected:
  const std::string raw_device_id_;
  const std::string device_name_;
  bool is_highlighted_ = false;
  const raw_ref<const gfx::VectorIcon> icon_;
};

class AudioDeviceEntryView : public DeviceEntryUI, public HoverButton {
  METADATA_HEADER(AudioDeviceEntryView, HoverButton)

 public:
  AudioDeviceEntryView(PressedCallback callback,
                       SkColor foreground_color,
                       SkColor background_color,
                       const std::string& raw_device_id,
                       const std::string& name);
  ~AudioDeviceEntryView() override = default;

  // DeviceEntryUI
  void OnColorsChanged(SkColor foreground_color,
                       SkColor background_color) override;
  DeviceEntryUIType GetType() const override;

  void SetHighlighted(bool highlighted);
  bool GetHighlighted() const;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_ENTRY_UI_H_
