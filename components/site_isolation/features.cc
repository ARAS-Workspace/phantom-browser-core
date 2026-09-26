// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_isolation/features.h"

#include "build/build_config.h"

namespace site_isolation {
namespace features {

// Controls a mode for dynamically process-isolating sites where the user has
// entered a password.  This is intended to be used primarily when full site
// isolation is turned off.  To check whether this mode is enabled, use
// SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled() rather than
// checking the feature directly, since that decision is influenced by other
// factors as well.
BASE_FEATURE(
    kSiteIsolationForPasswordSites,
    "site-isolation-for-password-sites",
    // Enabled by default on Android; see https://crbug.com/849815.  Note that
    // this should not affect Android Webview, which does not include this code.
    base::FEATURE_DISABLED_BY_DEFAULT);

// Controls a mode for dynamically process-isolating sites where the user has
// logged in via OAuth.  These sites are determined by runtime heuristics.
//
// This is intended to be used primarily when full site isolation is turned
// off.  To check whether this mode is enabled, use
// SiteIsolationPolicy::IsIsolationForOAuthSitesEnabled() rather than
// checking the feature directly, since that decision is influenced by other
// factors as well.
//
// This feature does not affect Android Webview, which does not include this
// code.
BASE_FEATURE(
    kSiteIsolationForOAuthSites,
    // Enabled by default on Android only; see https://crbug.com/1206770.
    base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the rollout of the IsolateOriginsShortlist policy, which allows
// isolating a shortlist of critical origins on resource-constrained devices
// (like low-end Android phones) instead of the full IsolateOrigins list.
BASE_FEATURE(kIsolateOriginsShortlist,
             "IsolateOriginsShortlistPolicyUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// In order to have broader support for JavaScript optimizer exceptions, we'll
// apply origin isolation on navigation for URLs that match rules in the
// JAVASCRIPT_OPTIMIZER content setting that don't match the default setting.
// TODO(crbug.com/413695645): we want this feature to start isolating any origin
// that was specified in the rules but there currently isn't a way to determine
// this from the host_content_settings_map API, so we'll improve this in the
// future.
//
// On Android, this implicitly starts isolating origins under partial site
// isolation. If full site isolation is enabled (examples: through opt-in or an
// enterprise policy), then this feature behaves the same as on Desktop.
//
// On Desktop, this feature starts isolating origins where the custom policy
// targets an origin that is not already origin-isolated.
BASE_FEATURE(kOriginIsolationForJsOptExceptions,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Desktop-only: kOriginIsolationMemoryThreshold (if enabled) is used to
// determine if a device has the necessary resources to participate in origin
// isolation (a stronger model than site isolation, but with potentially higher
// resource usage).
BASE_FEATURE(kOriginIsolationMemoryThreshold,
             base::FEATURE_DISABLED_BY_DEFAULT);
const char kOriginIsolationMemoryThresholdParamName[] =
    "origin_isolation_threshold_mb";

}  // namespace features
}  // namespace site_isolation
