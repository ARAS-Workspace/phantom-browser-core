// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/cross_device/theme_translation.h"

#include "build/build_config.h"
#include "build/buildflag.h"

namespace themes {

DeviceThemeInfo<sync_pb::ThemeSpecifics> TranslateAndroid(
    const sync_pb::ThemeAndroidSpecifics& android_specifics) {
  DeviceThemeInfo<sync_pb::ThemeSpecifics> info;
  info.os_type = syncer::DeviceInfo::OsType::kAndroid;

  if (android_specifics.has_use_custom_theme()) {
    info.theme.set_use_custom_theme(android_specifics.use_custom_theme());
  }
  if (android_specifics.has_ntp_background()) {
    *info.theme.mutable_ntp_background() = android_specifics.ntp_background();
  }
  if (android_specifics.has_user_color_theme()) {
    *info.theme.mutable_user_color_theme() =
        android_specifics.user_color_theme();
  }
  return info;
}

DeviceThemeInfo<sync_pb::ThemeSpecifics> TranslateIos(
    const sync_pb::ThemeIosSpecifics& ios_specifics) {
  DeviceThemeInfo<sync_pb::ThemeSpecifics> info;
  info.os_type = syncer::DeviceInfo::OsType::kIOS;

  if (ios_specifics.has_user_color_theme()) {
    *info.theme.mutable_user_color_theme() = ios_specifics.user_color_theme();
  }
  if (ios_specifics.has_ntp_background()) {
    *info.theme.mutable_ntp_background() = ios_specifics.ntp_background();
  }
  return info;
}

}  // namespace themes
