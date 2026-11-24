// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

namespace subresource_filter {

// Constant definitions -------------------------------------------------------

BASE_FEATURE(kSafeBrowsingSubresourceFilter,
             "SubresourceFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace subresource_filter
