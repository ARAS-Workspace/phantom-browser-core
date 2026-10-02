// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_PREFS_H_
#define COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_PREFS_H_

class PrefRegistrySimple;

namespace enterprise_connectors {

inline constexpr const char kWatermarkStyleFillOpacityPref[] =
    "policy.watermark_style.fill_opacity";
inline constexpr const char kWatermarkStyleOutlineOpacityPref[] =
    "policy.watermark_style.outline_opacity";
inline constexpr const char kWatermarkStyleFontSizePref[] =
    "policy.watermark_style.font_size";
inline constexpr const char kWatermarkStyleTimestampTimezonePref[] =
    "policy.watermark_style.timestamp_timezone";
inline constexpr const char kWatermarkStyleFillOpacityFieldName[] =
    "fill_opacity";
inline constexpr const char kWatermarkStyleOutlineOpacityFieldName[] =
    "outline_opacity";
inline constexpr const char kWatermarkStyleFontSizeFieldName[] = "font_size";
inline constexpr const char kWatermarkStyleTimestampTimezoneFieldName[] =
    "timestamp_timezone";

// Different tuned default values are set for mobiles and Desktops based on
// their screen sizes, screen resolutions etc.
inline constexpr int kWatermarkStyleFillOpacityDefault = 4;
inline constexpr int kWatermarkStyleOutlineOpacityDefault = 6;
inline constexpr int kWatermarkStyleFontSizeDefault = 24;
inline constexpr const char kWatermarkStyleTimestampTimezoneDefault[] =
    "user_device";

void RegisterWatermarkProfilePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_PREFS_H_
