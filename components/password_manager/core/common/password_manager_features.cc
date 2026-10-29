// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_features.h"

#include "base/feature_list.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"

namespace password_manager::features {
// NOTE: It is strongly recommended to use UpperCamelCase style for feature
//       names, e.g. "MyGreatFeature".

// When enabled, will inform browser about filling through `FillField` as user
// input.
BASE_FEATURE(kActorLoginTreatFillingAsUserInput,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Removes password suggestion filtering by username.
BASE_FEATURE(kNoPasswordSuggestionFiltering, base::FEATURE_DISABLED_BY_DEFAULT);

// Allows to show suggestions automatically when password forms are autofocused
// on pageload. Enabled by default on desktop in M140.
BASE_FEATURE(kShowSuggestionsOnAutofocus, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace password_manager::features
