// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/features/features.h"

#include "base/metrics/field_trial_params.h"

namespace payments::facilitated {

// When enabled, Chrome will offer to pay with accounts supporting Pix to users
// using their devices in landscape mode. Chrome always offers to pay with Pix
// accounts for users using their devices in portrait mode.
BASE_FEATURE(kEnablePixPaymentsInLandscapeMode,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the Rust implementation of the Pix code validator.
BASE_FEATURE(kUseRustPixCodeValidator, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace payments::facilitated
