// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/features.h"

#include "base/feature_list.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

namespace policy::features {

BASE_FEATURE(kProfileSeparationDomainExceptionListRetroactive,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, uses ManagementService to determine whether to honor sensitive
// policies. When disabled, falls back to the original ShouldHonorPolicies()
// behavior.
BASE_FEATURE(kUseManagementServiceForSensitivePolicies,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFilterSensitivePoliciesOnWorkplaceJoinedDevices,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Modifies behavior of policies utilizing URLBlocklistManager.
// When enabled, bypasses the wildcard "*" in the blocklist for internal
// chrome:// URLs such as chrome://ntp, chrome://bookmarks, etc.
BASE_FEATURE(kBypassURLBlocklistWildcardForInternalChromeUrls,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Modifies behavior of policies utilizing URLBlocklistManager.
// When enabled, downgrades the match level to neutral if the URL is allowed by
// the wildcard '*' in the allowlist.
BASE_FEATURE(kDowngradeURLAllowlistWildcardToNeutral,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPolicyPageMojoMigration, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kURLBlocklistOverridesIncognitoAllowlist,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace policy::features
