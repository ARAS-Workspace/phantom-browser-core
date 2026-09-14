// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/common/translate_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

translate::mojom::TranslateError EnumTraits<
    translate::mojom::TranslateError,
    translate::TranslateErrors>::ToMojom(translate::TranslateErrors input) {
  switch (input) {
    case translate::TranslateErrors::NONE:
      return translate::mojom::TranslateError::NONE;
    case translate::TranslateErrors::NETWORK:
      return translate::mojom::TranslateError::NETWORK;
    case translate::TranslateErrors::INITIALIZATION_ERROR:
      return translate::mojom::TranslateError::INITIALIZATION_ERROR;
    case translate::TranslateErrors::UNKNOWN_LANGUAGE:
      return translate::mojom::TranslateError::UNKNOWN_LANGUAGE;
    case translate::TranslateErrors::UNSUPPORTED_LANGUAGE:
      return translate::mojom::TranslateError::UNSUPPORTED_LANGUAGE;
    case translate::TranslateErrors::IDENTICAL_LANGUAGES:
      return translate::mojom::TranslateError::IDENTICAL_LANGUAGES;
    case translate::TranslateErrors::TRANSLATION_ERROR:
      return translate::mojom::TranslateError::TRANSLATION_ERROR;
    case translate::TranslateErrors::TRANSLATION_TIMEOUT:
      return translate::mojom::TranslateError::TRANSLATION_TIMEOUT;
    case translate::TranslateErrors::UNEXPECTED_SCRIPT_ERROR:
      return translate::mojom::TranslateError::UNEXPECTED_SCRIPT_ERROR;
    case translate::TranslateErrors::BAD_ORIGIN:
      return translate::mojom::TranslateError::BAD_ORIGIN;
    case translate::TranslateErrors::SCRIPT_LOAD_ERROR:
      return translate::mojom::TranslateError::SCRIPT_LOAD_ERROR;
    case translate::TranslateErrors::TRANSLATE_ERROR_MAX:
      return translate::mojom::TranslateError::TRANSLATE_ERROR_MAX;
  }

  NOTREACHED();
}

translate::TranslateErrors
EnumTraits<translate::mojom::TranslateError, translate::TranslateErrors>::
    FromMojom(translate::mojom::TranslateError input) {
  switch (input) {
    case translate::mojom::TranslateError::NONE:
      return translate::TranslateErrors::NONE;
    case translate::mojom::TranslateError::NETWORK:
      return translate::TranslateErrors::NETWORK;
    case translate::mojom::TranslateError::INITIALIZATION_ERROR:
      return translate::TranslateErrors::INITIALIZATION_ERROR;
    case translate::mojom::TranslateError::UNKNOWN_LANGUAGE:
      return translate::TranslateErrors::UNKNOWN_LANGUAGE;
    case translate::mojom::TranslateError::UNSUPPORTED_LANGUAGE:
      return translate::TranslateErrors::UNSUPPORTED_LANGUAGE;
    case translate::mojom::TranslateError::IDENTICAL_LANGUAGES:
      return translate::TranslateErrors::IDENTICAL_LANGUAGES;
    case translate::mojom::TranslateError::TRANSLATION_ERROR:
      return translate::TranslateErrors::TRANSLATION_ERROR;
    case translate::mojom::TranslateError::TRANSLATION_TIMEOUT:
      return translate::TranslateErrors::TRANSLATION_TIMEOUT;
    case translate::mojom::TranslateError::UNEXPECTED_SCRIPT_ERROR:
      return translate::TranslateErrors::UNEXPECTED_SCRIPT_ERROR;
    case translate::mojom::TranslateError::BAD_ORIGIN:
      return translate::TranslateErrors::BAD_ORIGIN;
    case translate::mojom::TranslateError::SCRIPT_LOAD_ERROR:
      return translate::TranslateErrors::SCRIPT_LOAD_ERROR;
    case translate::mojom::TranslateError::TRANSLATE_ERROR_MAX:
      return translate::TranslateErrors::TRANSLATE_ERROR_MAX;
  }

  NOTREACHED();
}

}  // namespace mojo
