// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace subresource_filter {

// Constant definitions -------------------------------------------------------

BASE_FEATURE(kSafeBrowsingSubresourceFilter,
             "SubresourceFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAdsInterventionsEnforced, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kAdsInterventionDuration = {
    &kAdsInterventionsEnforced, "kAdsInterventionDuration", base::Days(3)};

}  // namespace subresource_filter
