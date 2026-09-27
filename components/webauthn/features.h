// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_FEATURES_H_
#define COMPONENTS_WEBAUTHN_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace webauthn::features {

// Controls deletion of passkeys that have been hidden for a while.
BASE_DECLARE_FEATURE(kDeleteOldHiddenPasskeys);

// Reject RP IDs inside the caller's public suffix.
BASE_DECLARE_FEATURE(kRejectRpIdsInsideCallersPublicSuffix);

}  // namespace webauthn::features

#endif  // COMPONENTS_WEBAUTHN_FEATURES_H_
