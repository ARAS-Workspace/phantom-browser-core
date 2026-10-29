// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace password_manager::features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
BASE_DECLARE_FEATURE(kActorLoginTreatFillingAsUserInput);
BASE_DECLARE_FEATURE(kNoPasswordSuggestionFiltering);
BASE_DECLARE_FEATURE(kShowSuggestionsOnAutofocus);

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
