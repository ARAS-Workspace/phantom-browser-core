// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/ntp_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace ntp_features {

// If enabled, shows wallpaper search within the Customize Chrome Side Panel.
// This is a kill switch. Keep indefinitely.
BASE_FEATURE(kCustomizeChromeWallpaperSearch, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, shows entry point on Customize Chrome Side Panel's Appearance
// page for Wallpaper Search.";
BASE_FEATURE(kCustomizeChromeWallpaperSearchButton,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shows inspiration card in Customize Chrome Side Panel Wallpaper
// Search.
BASE_FEATURE(kCustomizeChromeWallpaperSearchInspirationCard,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the EnergyEffect for Realbox will be shown.
BASE_FEATURE(kEnergyEffect, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, NTP "realbox" will be themed for CR23. Includes realbox
// matching omnibox theme and increased realbox shadow.
BASE_FEATURE(kRealboxCr23Theming,
             "NtpRealboxCr23Theming",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, alpha NTP backgrounds will show in Customize Chrome.
// This is a development switch. Keep indefinitely.
BASE_FEATURE(kNtpAlphaBackgroundCollections, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, NTP background image links will be checked for HTTP status
// errors."
BASE_FEATURE(kNtpBackgroundImageErrorDetection,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, ChromeContentBrowserClient::OverrideNavigationParams no
// longer treats NTP-sourced renderer-initiated link clicks as browser-
// initiated.
BASE_FEATURE(kNtpDisableBrowserInitiatedLinks,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shortcuts will be shown.
// This is a kill switch. Keep indefinitely.
BASE_FEATURE(kNtpShortcuts, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, high-DPI favicons are fetched for Most Visited tiles.
BASE_FEATURE(kNtpMostVisitedHighDpiFavicons, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the features of NTP Next (AI action chips etc.) will be shown.
BASE_FEATURE(kNtpNextFeatures, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, a footer will show on the NTP.
BASE_FEATURE(kNtpFooter, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, stale shortcuts will be auto-removed from the NTP.
BASE_FEATURE(kNtpFeatureOptimizationShortcutsRemoval,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the shortcuts will be redesigned.
BASE_FEATURE(kNtpShortcutsRedesign, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the bookmark bar may be auto-removed on the NTP and new
// visibility settings are added.
BASE_FEATURE(kNtpSimplificationBookmarkBar, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the bookmark bar time interval and number of times it's shown on
// the NTP before auto-hiding is decreased for testing.
BASE_FEATURE(kBookmarkBarUpdatesForTesting, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kNtpRealboxCr23ExpandedStateBgMatchesOmnibox(
    &ntp_features::kRealboxCr23Theming,
    "kNtpRealboxCr23ExpandedStateBgMatchesOmnibox",
    true);
const base::FeatureParam<bool> kNtpRealboxCr23SteadyStateShadow(
    &ntp_features::kRealboxCr23Theming,
    "kNtpRealboxCr23SteadyStateShadow",
    false);

const base::FeatureParam<base::TimeDelta>
    kShortcutsMinStalenessUpdateTimeInterval(
        &ntp_features::kNtpFeatureOptimizationShortcutsRemoval,
        "ShortcutsMinStalenessUpdateTimeInterval",
        base::Days(1));
const base::FeatureParam<int> kStaleShortcutsCountThreshold(
    &ntp_features::kNtpFeatureOptimizationShortcutsRemoval,
    "StaleShortcutsCountThreshold",
    60);

const base::FeatureParam<int> kMaxTilesInCollapsedState{
    &ntp_features::kNtpShortcutsRedesign, "max_tiles_in_collapsed_state", 4};
const base::FeatureParam<int> kMaxShortcutsInExpandedState{
    &ntp_features::kNtpShortcutsRedesign, "max_shortcuts_in_expanded_state",
    20};
const base::FeatureParam<int> kMaxMostVisitedTilesInExpandedState{
    &ntp_features::kNtpShortcutsRedesign,
    "max_most_visited_tiles_in_expanded_state", 10};
const base::FeatureParam<int> kMaxEnterpriseShortcuts{
    &ntp_features::kNtpShortcutsRedesign, "max_enterprise_shortcuts", 10};

const base::FeatureParam<int> kBookmarkBarCountThreshold{
    &ntp_features::kNtpSimplificationBookmarkBar, "BookmarkBarCountThreshold",
    15};
const base::FeatureParam<base::TimeDelta> kBookmarkBarMinStalenessTimeInterval(
    &ntp_features::kNtpSimplificationBookmarkBar,
    "BookmarkBarMinStalenessTimeInterval",
    base::Days(1));

int GetMaxTilesInCollapsedState() {
  if (!base::FeatureList::IsEnabled(kNtpShortcutsRedesign)) {
    return 6;
  }
  return kMaxTilesInCollapsedState.Get();
}

int GetMaxShortcutsInExpandedState() {
  if (!base::FeatureList::IsEnabled(kNtpShortcutsRedesign)) {
    return 10;
  }
  return kMaxShortcutsInExpandedState.Get();
}

int GetMaxMostVisitedTilesInExpandedState() {
  if (!base::FeatureList::IsEnabled(kNtpShortcutsRedesign)) {
    return 8;
  }
  return kMaxMostVisitedTilesInExpandedState.Get();
}

int GetMaxEnterpriseShortcuts() {
  return kMaxEnterpriseShortcuts.Get();
}
base::TimeDelta GetBookmarkBarMinStalenessTimeInterval() {
  if (base::FeatureList::IsEnabled(kBookmarkBarUpdatesForTesting)) {
    return base::Seconds(15);
  }
  return kBookmarkBarMinStalenessTimeInterval.Get();
}
int GetBookmarkBarCountThreshold() {
  if (base::FeatureList::IsEnabled(kBookmarkBarUpdatesForTesting)) {
    return 1;
  }
  return kBookmarkBarCountThreshold.Get();
}

}  // namespace ntp_features
