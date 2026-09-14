// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_COMMON_TRANSLATE_MOJOM_TRAITS_H_
#define COMPONENTS_TRANSLATE_CONTENT_COMMON_TRANSLATE_MOJOM_TRAITS_H_

#include "components/translate/content/common/translate.mojom-shared.h"
#include "components/translate/core/common/translate_errors.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<translate::mojom::TranslateError,
                  translate::TranslateErrors> {
  static translate::mojom::TranslateError ToMojom(
      translate::TranslateErrors input);
  static translate::TranslateErrors FromMojom(
      translate::mojom::TranslateError input);
};

}  // namespace mojo

#endif  // COMPONENTS_TRANSLATE_CONTENT_COMMON_TRANSLATE_MOJOM_TRAITS_H_
