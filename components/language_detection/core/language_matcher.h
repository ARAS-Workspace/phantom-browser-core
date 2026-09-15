// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_MATCHER_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_MATCHER_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/language_tag_matcher.h"

namespace language_detection {

// Returns the list of default supported languages as LanguageTag elements.
COMPONENT_EXPORT(LANGUAGE_DETECTION)
base::span<const base::i18n::LanguageTag> GetDefaultSupportedLanguages();

// Returns the LanguageTagMatcher initialized with the default supported
// languages.
COMPONENT_EXPORT(LANGUAGE_DETECTION)
const base::i18n::LanguageTagMatcherWithDefault& GetSupportedLanguageMatcher();

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_MATCHER_H_
