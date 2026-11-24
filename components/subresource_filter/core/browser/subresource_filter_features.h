// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace subresource_filter {

// Feature and variation parameter definitions -------------------------------

// The primary toggle to enable/disable the Safe Browsing Subresource Filter.
BASE_DECLARE_FEATURE(kSafeBrowsingSubresourceFilter);

// Enables the blocking of ads on sites that have ads violations.
BASE_DECLARE_FEATURE(kAdsInterventionsEnforced);

// The maximum duration that an ads intervention is active for.
// TODO(crbug.com/40721691): This currently is the default delay.
// We should move to an approach where each intervention has a duration that is
// attainable separately as a parameter for that intervention. Right now this is
// overridden explicitly in a switch for interventions that require a different
// default duration.
extern const base::FeatureParam<base::TimeDelta> kAdsInterventionDuration;

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_H_
