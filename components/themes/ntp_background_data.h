// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_NTP_BACKGROUND_DATA_H_
#define COMPONENTS_THEMES_NTP_BACKGROUND_DATA_H_

#include <optional>

#include "third_party/skia/include/core/SkColor.h"

// Represents a custom background on the new tab page.
struct CustomBackground {
  CustomBackground();
  CustomBackground(const CustomBackground&);
  CustomBackground(CustomBackground&&);
  ~CustomBackground();

  CustomBackground& operator=(const CustomBackground&);
  CustomBackground& operator=(CustomBackground&&);

  // Main color of the image.
  std::optional<SkColor> custom_background_main_color;
};

#endif  // COMPONENTS_THEMES_NTP_BACKGROUND_DATA_H_
