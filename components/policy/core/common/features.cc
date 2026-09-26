// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

namespace policy::features {

BASE_FEATURE(kProfileSeparationDomainExceptionListRetroactive,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnhancedSecurityEventFields, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseCECFlagInPolicyData, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a configurable delay for policy registration.
BASE_FEATURE(kCustomPolicyRegistrationDelay, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kPolicyRegistrationDelay{
    &kCustomPolicyRegistrationDelay, "PolicyRegistrationDelay", base::Hours(6)};

// Used to enable extension install policy support.
BASE_FEATURE(kEnableExtensionInstallPolicyFetching,
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

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

BASE_FEATURE(kDeviceSignalsBackfillDisclaimer,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kClearDeviceSignalsPermissionOnStartup{
    &kDeviceSignalsBackfillDisclaimer,
    "clear_device_signals_permission_on_startup", false};

BASE_FEATURE(kURLBlocklistOverridesIncognitoAllowlist,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExportPlatformPoliciesJson,
             "ExportPlatformPoliciesJson",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, migrates user cloud management status and sign-in interception
// policy fetching from the legacy SecureConnect endpoint to Device Management
// Server.
BASE_FEATURE(kMigrateSecureConnectApiToDmServer,
             "MigrateSecureConnectApiToDmServer",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kMigrateSecureConnectApiToDmServerFetchTimeout{
        &kMigrateSecureConnectApiToDmServer, "fetch_timeout", base::Seconds(10)};

}  // namespace policy::features
