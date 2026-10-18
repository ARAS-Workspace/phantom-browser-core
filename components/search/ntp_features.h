// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_NTP_FEATURES_H_
#define COMPONENTS_SEARCH_NTP_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ntp_features {

// The features should be documented alongside the definition of their values in
// the .cc file.

BASE_DECLARE_FEATURE(kCustomizeChromeSidePanelExtensionsCard);
BASE_DECLARE_FEATURE(kCustomizeChromeWallpaperSearch);
BASE_DECLARE_FEATURE(kCustomizeChromeWallpaperSearchButton);
BASE_DECLARE_FEATURE(kCustomizeChromeWallpaperSearchInspirationCard);
BASE_DECLARE_FEATURE(kEnergyEffect);
BASE_DECLARE_FEATURE(kRealboxCr23Theming);
BASE_DECLARE_FEATURE(kNtpAlphaBackgroundCollections);
BASE_DECLARE_FEATURE(kNtpBackgroundImageErrorDetection);
BASE_DECLARE_FEATURE(kNtpCustomizeChromeAutoOpen);
BASE_DECLARE_FEATURE(kNtpDisableBrowserInitiatedLinks);
BASE_DECLARE_FEATURE(kNtpShortcuts);
BASE_DECLARE_FEATURE(kNtpMostVisitedHighDpiFavicons);
BASE_DECLARE_FEATURE(kNtpNextFeatures);
BASE_DECLARE_FEATURE(kNtpFooter);
BASE_DECLARE_FEATURE(kNtpFeatureOptimizationShortcutsRemoval);
BASE_DECLARE_FEATURE(kNtpShortcutsRedesign);
BASE_DECLARE_FEATURE(kNtpSimplificationBookmarkBar);
BASE_DECLARE_FEATURE(kBookmarkBarUpdatesForTesting);

// Parameter for the maximum number of times to automatically show
// Customize Chrome.
extern const base::FeatureParam<int> kNtpCustomizeChromeAutoShownMaxCount;
// Parameter for the maximum number of times to automatically show
// Customize Chrome in a session.
extern const base::FeatureParam<int>
    kNtpCustomizeChromeAutoShownSessionMaxCount;
// Parameter determining the background color of the expanded state realbox.
extern const base::FeatureParam<bool>
    kNtpRealboxCr23ExpandedStateBgMatchesOmnibox;
// Parameter determining the whether the steady state realbox has a shadow.
extern const base::FeatureParam<bool> kNtpRealboxCr23SteadyStateShadow;

// Parameter determining if the disablement is enabled for NTP Next.
extern const base::FeatureParam<bool> kNtpNextDisablementParam;

// Parameter determining the minimum amount of time that must pass before
// shortcuts staleness counters will be incremented.
extern const base::FeatureParam<base::TimeDelta>
    kShortcutsMinStalenessUpdateTimeInterval;

// Parameter determining the count at which shortcuts will be considered stale
// and be eligible for auto-removal.
extern const base::FeatureParam<int> kStaleShortcutsCountThreshold;

// Parameter determining the count at which the bookmark bar will be eligible
// for auto-removal.
extern const base::FeatureParam<int> kBookmarkBarCountThreshold;

// Parameter determining the minimum amount of time before auto-removal prefs
// are updated.
extern const base::FeatureParam<base::TimeDelta>
    kBookmarkBarMinStalenessTimeInterval;

// Parameter determining the max number of tiles to show in the collapsed state.
extern const base::FeatureParam<int> kMaxTilesInCollapsedState;

// Parameter determining the max number of shortcuts to show in the expanded
// state.
extern const base::FeatureParam<int> kMaxShortcutsInExpandedState;

// Parameter determining the max number of MV tiles to show in the expanded
// state.
extern const base::FeatureParam<int> kMaxMostVisitedTilesInExpandedState;

// Parameter determining the max number of enterprise shortcuts allowed.
extern const base::FeatureParam<int> kMaxEnterpriseShortcuts;

// Returns the max number of tiles to show in the collapsed state.
int GetMaxTilesInCollapsedState();

// Returns the max number of shortcuts to show in the expanded state.
int GetMaxShortcutsInExpandedState();

// Returns the max number of MV tiles to show in the expanded state.
int GetMaxMostVisitedTilesInExpandedState();

// Returns the max number of enterprise shortcuts allowed.
int GetMaxEnterpriseShortcuts();

base::TimeDelta GetBookmarkBarMinStalenessTimeInterval();

int GetBookmarkBarCountThreshold();

}  // namespace ntp_features

#endif  // COMPONENTS_SEARCH_NTP_FEATURES_H_
