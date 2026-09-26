// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_switches.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace switches {

bool IsDynamicProfileCountryEnabled() {
  return base::FeatureList::IsEnabled(kDynamicProfileCountry);
}

BASE_FEATURE(kDynamicProfileCountry, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCurrentDseHighlightOnChoiceScreenSupport,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWaffleRestrictToAssociatedCountries,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStrictAssociatedCountriesCheck,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrepopulatedEnginesMigration, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrepopulatedEnginesShadowVariants,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool ArePrepopulatedEnginesShadowVariantsEnabled() {
  return base::FeatureList::IsEnabled(kPrepopulatedEnginesShadowVariants);
}

}  // namespace switches
