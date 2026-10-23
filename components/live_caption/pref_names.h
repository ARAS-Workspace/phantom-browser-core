// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_PREF_NAMES_H_
#define COMPONENTS_LIVE_CAPTION_PREF_NAMES_H_

namespace prefs {

// These kAccessibilityCaptions* caption style prefs are used on Android
// (though their primary use is for Live Caption, which is why they are housed
// within this live_caption component instead of somewhere more generic).

// String indicating the size of the captions text as a percentage.
inline constexpr char kAccessibilityCaptionsTextSize[] =
    "accessibility.captions.text_size";

// String indicating the font of the captions text.
inline constexpr char kAccessibilityCaptionsTextFont[] =
    "accessibility.captions.text_font";

// Comma-separated string indicating the RGB values of the captions text color.
inline constexpr char kAccessibilityCaptionsTextColor[] =
    "accessibility.captions.text_color";

// Integer indicating the opacity of the captions text from 0 - 100.
inline constexpr char kAccessibilityCaptionsTextOpacity[] =
    "accessibility.captions.text_opacity";

// Comma-separated string indicating the RGB values of the background color.
inline constexpr char kAccessibilityCaptionsBackgroundColor[] =
    "accessibility.captions.background_color";

// CSS string indicating the shadow of the captions text.
inline constexpr char kAccessibilityCaptionsTextShadow[] =
    "accessibility.captions.text_shadow";

// Integer indicating the opacity of the captions text background from 0 - 100.
inline constexpr char kAccessibilityCaptionsBackgroundOpacity[] =
    "accessibility.captions.background_opacity";

}  // namespace prefs

#endif  // COMPONENTS_LIVE_CAPTION_PREF_NAMES_H_
