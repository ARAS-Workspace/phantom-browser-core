// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_METRICS_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_METRICS_H_

#include "base/component_export.h"

namespace language_detection {

// Internals exposed for testing purposes. Should not be relied on by client
// code.
namespace metrics_internal {

// Constant string value to indicate the UMA name.
COMPONENT_EXPORT(LANGUAGE_DETECTION)
extern const char kLanguageDetectionLanguageVerification[];

}  // namespace metrics_internal

// When a valid Content-Language is provided, the language detection agent
// checks if a server provided Content-Language matches to a language the
// model determined.
// This enum is used for recording metrics. This enum should remain synchronized
// with the enum "TranslateLanguageVerification" in enums.xml.
enum class LanguageVerificationType {
  // kModelDisabled = 0, -- obsolete
  kModelOnly = 1,
  kModelUnknown = 2,
  kModelAgrees = 3,
  kModelDisagrees = 4,
  kModelOverrides = 5,
  kModelComplementsCountry = 6,
  kNoPageContent = 7,
  kModelNotAvailable = 8,
  kModelHistogramBoundary = 9,
  kMaxValue = kModelHistogramBoundary,
};

// Called when CLD verifies Content-Language header.
COMPONENT_EXPORT(LANGUAGE_DETECTION)
void ReportLanguageVerification(LanguageVerificationType type);

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_METRICS_H_
