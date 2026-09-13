// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_UTIL_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_UTIL_H_

#include <optional>

#include "components/prefs/pref_service.h"
#include "ui/native_theme/caption_style.h"

class PrefService;

namespace captions {


std::optional<ui::CaptionStyle> GetCaptionStyleFromUserSettings(
    PrefService* prefs,
    bool record_metrics);


}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_UTIL_H_
