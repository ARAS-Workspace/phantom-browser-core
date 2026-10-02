// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/watermarking/watermark_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace enterprise_connectors {

void RegisterWatermarkProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kWatermarkStyleFillOpacityPref,
                                kWatermarkStyleFillOpacityDefault);
  registry->RegisterIntegerPref(kWatermarkStyleOutlineOpacityPref,
                                kWatermarkStyleOutlineOpacityDefault);
  registry->RegisterIntegerPref(kWatermarkStyleFontSizePref,
                                kWatermarkStyleFontSizeDefault);
  registry->RegisterStringPref(kWatermarkStyleTimestampTimezonePref,
                               kWatermarkStyleTimestampTimezoneDefault);
}

}  // namespace enterprise_connectors
