// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/content/common/language_detection_mojom_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<language_detection::mojom::LanguageDetectionDetailsDataView,
                  language_detection::LanguageDetectionDetails>::
    Read(language_detection::mojom::LanguageDetectionDetailsDataView data,
         language_detection::LanguageDetectionDetails* out) {
  out->has_run_lang_detection = data.has_run_lang_detection();

  if (!data.ReadTime(&out->time)) {
    return false;
  }
  if (!data.ReadUrl(&out->url)) {
    return false;
  }
  if (!data.ReadContentLanguage(&out->content_language)) {
    return false;
  }
  if (!data.ReadModelDetectedLanguage(&out->model_detected_language)) {
    return false;
  }

  out->is_model_reliable = data.is_model_reliable();
  out->has_notranslate = data.has_notranslate();

  if (!data.ReadHtmlRootLanguage(&out->html_root_language)) {
    return false;
  }
  if (!data.ReadAdoptedLanguage(&out->adopted_language)) {
    return false;
  }
  if (!data.ReadContents(&out->contents)) {
    return false;
  }

  out->model_reliability_score = data.model_reliability_score();
  if (!data.ReadDetectionModelVersion(&out->detection_model_version)) {
    return false;
  }

  return true;
}

}  // namespace mojo
