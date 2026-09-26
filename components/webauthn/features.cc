// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace webauthn::features {

// Not yet enabled by default.
BASE_FEATURE(kDeleteOldHiddenPasskeys, base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled by default in M152. Remove in or after M155.
BASE_FEATURE(kRejectRpIdsInsideCallersPublicSuffix,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace webauthn::features
