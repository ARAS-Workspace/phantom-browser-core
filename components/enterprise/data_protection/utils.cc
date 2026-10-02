// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_protection/utils.h"

#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/i18n/timezone.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/enterprise/data_protection/features.h"
#include "components/enterprise/watermarking/watermark_prefs.h"

namespace enterprise_data_protection {

UrlSettings::UrlSettings() = default;
UrlSettings::UrlSettings(const UrlSettings&) = default;
UrlSettings& UrlSettings::operator=(const UrlSettings&) = default;
UrlSettings::~UrlSettings() = default;

bool UrlSettings::operator==(const UrlSettings& other) const {
  return watermark_text == other.watermark_text &&
         allow_screenshots == other.allow_screenshots;
}

// static
const UrlSettings& UrlSettings::None() {
  static base::NoDestructor<UrlSettings> empty;
  return *empty.get();
}

std::string FormatWatermarkTimestamp(
    const base::Time& time,
    const std::optional<std::string>& timestamp_timezone) {
  base::i18n::TimeZone timezone = base::i18n::TimeZone::Default();
  if (timestamp_timezone.has_value() &&
      *timestamp_timezone !=
          enterprise_connectors::kWatermarkStyleTimestampTimezoneDefault) {
    timezone = base::i18n::TimeZone::FromString(*timestamp_timezone);
  }

  return base::TimeFormatAsIso8601(
      time, timezone,
      base::i18n::DateTimeFormatterOptions::TimePrecision::kSecond,
      /*include_offset_suffix=*/true);
}

}  // namespace enterprise_data_protection
