// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font_data/local_font_matcher.h"

#include <memory>

#include "build/build_config.h"

namespace font_data_service {

// static
std::unique_ptr<LocalFontMatcher> LocalFontMatcher::Create() {
  return nullptr;
}

}  // namespace font_data_service
