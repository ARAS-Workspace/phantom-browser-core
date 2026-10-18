// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/features.h"

#include "base/feature_list.h"

namespace permissions {
namespace features {

// Enables or disables whether pages with pending permission requests will
// go into back/forward cache.
BASE_FEATURE(kBackForwardCacheUnblockPermissionRequest,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kKeyboardLockPrompt, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables different positioning of the permission dialog, so that it's placed
// near the permission element, if possible.
// This feature should be enabled with blink::features::kPermissionElement.
BASE_FEATURE(kPermissionElementPromptPositioning,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables heuristic auto-grant for permissions.
BASE_FEATURE(kPermissionHeuristicAutoGrant, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionPredictionsV2, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionsAIv4, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionsAIP92, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionsAILikelihoodOrRelevance,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionOnDeviceNotificationPredictions,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionOnDeviceGeolocationPredictions,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPermissionPromiseLifetimeModulationAndroid,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, use the value of the `allowlist_urls` FeatureParam as the
// list of origins which would be allowed to access browser permission and
// device attribute API for a web kiosk session.
BASE_FEATURE(kAllowMultipleOriginsForWebKioskPermissions,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, chooser permissions grants will have a last visited timestamp
// date set. The timestamp will be later used to auto-revoke the permission,
// if eligible.
BASE_FEATURE(kRecordChooserPermissionLastVisitedTimestamps,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, blocks condition to exclude auto granted permissions for
// storage access exceptions. This will allow RWS permission grants to be
// visible in the Embedded content settings page.
BASE_FEATURE(kShowRelatedWebsiteSetsPermissionGrants,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCpssUseTfliteSignatureRunner, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyHubUnusedPermissionRevocationForAllSurfaces,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Only applicable if kApproximateGeolocationPermission is enabled. When
// enabled, will use the returned accuracy from CPSS to select the initial value
// of the geolocation accuracy (precise/approximate) for geolocation permission
// prompts.
BASE_FEATURE(kPermissionPredictionsGeolocationAccuracy,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, notification and geolocation permission requests that
// are not accompanied by a user gesture will be shown as quiet prompts.
BASE_FEATURE(kPermissionsGestureGatedPrompts,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace feature_params {

const base::FeatureParam<PermissionElementPromptPosition>::Option
    kPromptPositioningOptions[] = {
        {PermissionElementPromptPosition::kWindowMiddle, "window_middle"},
        {PermissionElementPromptPosition::kNearElement, "near_element"},
        {PermissionElementPromptPosition::kLegacyPrompt, "legacy_prompt"}};

const base::FeatureParam<PermissionElementPromptPosition>
    kPermissionElementPromptPositioningParam = {
        &features::kPermissionElementPromptPositioning,
        "PermissionElementPromptPositioningParam",
        PermissionElementPromptPosition::kWindowMiddle,
        &kPromptPositioningOptions};

const base::FeatureParam<double> kPermissionPredictionsV2HoldbackChance(
    &features::kPermissionPredictionsV2,
    "holdback_chance",
    0.3);

// Comma separated url patterns which should be allowed for accessing web kiosk
// browser permissions and device attributes API. If left empty no URL patterns
// will be allowed.
const base::FeatureParam<std::string> kWebKioskBrowserPermissionsAllowlist{
    &permissions::features::kAllowMultipleOriginsForWebKioskPermissions,
    "allowlist_urls", ""};

const base::FeatureParam<bool> kPermissionsGestureGatedPromptsMuteNotifications{
    &permissions::features::kPermissionsGestureGatedPrompts,
    "mute_notifications", false};

const base::FeatureParam<bool> kPermissionsGestureGatedPromptsMuteGeolocation{
    &permissions::features::kPermissionsGestureGatedPrompts, "mute_geolocation",
    false};

const base::FeatureParam<bool>
    kPermissionsGestureGatedPromptsExcludeSameOriginNavigations{
        &permissions::features::kPermissionsGestureGatedPrompts,
        "exclude_same_origin_navigations", false};

const base::FeatureParam<bool> kKeyboardLockPromptUIStyle{
    &permissions::features::kKeyboardLockPrompt, "use_pepc_ui", true};

}  // namespace feature_params
}  // namespace permissions
