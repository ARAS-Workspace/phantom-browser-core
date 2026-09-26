// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/features.h"

#include "build/build_config.h"
#include "components/history/core/browser/features.h"

namespace browsing_data::features {

BASE_FEATURE(kPasswordRemovalExtensionErrorKillSwitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace browsing_data::features
