// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_FEATURES_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace language_detection::features {

COMPONENT_EXPORT(LANGUAGE_DETECTION)
BASE_DECLARE_FEATURE(kLazyUpdateTranslateModel);

// Return whether TFLite-based language detection is enabled.
COMPONENT_EXPORT(LANGUAGE_DETECTION)
bool IsTFLiteLanguageDetectionEnabled();

}  // namespace language_detection::features

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_FEATURES_H_
