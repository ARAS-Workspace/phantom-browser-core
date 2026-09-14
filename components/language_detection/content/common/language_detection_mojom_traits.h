// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CONTENT_COMMON_LANGUAGE_DETECTION_MOJOM_TRAITS_H_
#define COMPONENTS_LANGUAGE_DETECTION_CONTENT_COMMON_LANGUAGE_DETECTION_MOJOM_TRAITS_H_

#include <string>

#include "base/time/time.h"
#include "components/language_detection/content/common/language_detection.mojom-shared.h"
#include "components/language_detection/core/language_detection_details.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<language_detection::mojom::LanguageDetectionDetailsDataView,
                    language_detection::LanguageDetectionDetails> {
  static bool has_run_lang_detection(
      const language_detection::LanguageDetectionDetails& r) {
    return r.has_run_lang_detection;
  }

  static const base::Time& time(
      const language_detection::LanguageDetectionDetails& r) {
    return r.time;
  }

  static const GURL& url(
      const language_detection::LanguageDetectionDetails& r) {
    return r.url;
  }

  static const std::string& content_language(
      const language_detection::LanguageDetectionDetails& r) {
    return r.content_language;
  }

  static const std::string& model_detected_language(
      const language_detection::LanguageDetectionDetails& r) {
    return r.model_detected_language;
  }

  static bool is_model_reliable(
      const language_detection::LanguageDetectionDetails& r) {
    return r.is_model_reliable;
  }

  static bool has_notranslate(
      const language_detection::LanguageDetectionDetails& r) {
    return r.has_notranslate;
  }

  static const std::string& html_root_language(
      const language_detection::LanguageDetectionDetails& r) {
    return r.html_root_language;
  }

  static const std::string& adopted_language(
      const language_detection::LanguageDetectionDetails& r) {
    return r.adopted_language;
  }

  static const std::u16string& contents(
      const language_detection::LanguageDetectionDetails& r) {
    return r.contents;
  }

  static float model_reliability_score(
      const language_detection::LanguageDetectionDetails& r) {
    return r.model_reliability_score;
  }

  static const std::string& detection_model_version(
      const language_detection::LanguageDetectionDetails& r) {
    return r.detection_model_version;
  }

  static bool Read(
      language_detection::mojom::LanguageDetectionDetailsDataView data,
      language_detection::LanguageDetectionDetails* out);
};

}  // namespace mojo

#endif  // COMPONENTS_LANGUAGE_DETECTION_CONTENT_COMMON_LANGUAGE_DETECTION_MOJOM_TRAITS_H_
