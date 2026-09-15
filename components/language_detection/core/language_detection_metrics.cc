// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/language_detection_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace language_detection {

namespace metrics_internal {

const char kLanguageDetectionLanguageVerification[] =
    "Translate.LanguageDetection.LanguageVerification";

}  // namespace metrics_internal

void ReportLanguageVerification(LanguageVerificationType type) {
  base::UmaHistogramEnumeration(
      metrics_internal::kLanguageDetectionLanguageVerification, type,
      LanguageVerificationType::kMaxValue);
}

}  // namespace language_detection
