// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/features.h"

#include "base/feature_list.h"

namespace webapps {
namespace features {

// Do not remove this feature flag, since it serves as a kill-switch for the ML
// promotion model. Kill switches are required for all ML model-backed features.
BASE_FEATURE(kWebAppsEnableMLModelForPromotion,
             base::FEATURE_DISABLED_BY_DEFAULT);
extern const base::FeatureParam<double> kWebAppsMLGuardrailResultReportProb(
    &kWebAppsEnableMLModelForPromotion,
    "guardrail_report_prob",
    0);
extern const base::FeatureParam<double> kWebAppsMLModelUserDeclineReportProb(
    &kWebAppsEnableMLModelForPromotion,
    "model_and_user_decline_report_prob",
    0);
extern const base::FeatureParam<int> kMaxDaysForMLPromotionGuardrailStorage(
    &kWebAppsEnableMLModelForPromotion,
    "max_days_to_store_guardrails",
    kTotalDaysToStoreMLGuardrails);

// Checking if a web app is installed in Chrome Android ultimately leads to a
// long, UI-thread Binder call. Enabling this flag makes the web app
// installation check on Clank async.
BASE_FEATURE(kCheckWebAppExistenceAsync, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
}  // namespace webapps
