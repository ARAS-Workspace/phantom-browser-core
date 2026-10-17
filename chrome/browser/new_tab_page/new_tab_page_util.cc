// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/new_tab_page_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/variations/service/variations_service.h"

namespace {

constexpr char kShortcutsAutoRemovalReasonHistogram[] =
    "NewTabPage.MostVisited.AutoRemovalSkipped";

bool IsInUS() {
  return g_browser_process->GetApplicationLocale() == "en-US" &&
         GetVariationsServiceCountryCode(
             g_browser_process->variations_service()) == "us";
}

}  // namespace

bool IsEnUSLocaleOnlyFeatureEnabled(const base::Feature& ntp_feature) {
  if (base::FeatureList::GetInstance()->IsFeatureOverridden(ntp_feature.name)) {
    return base::FeatureList::IsEnabled(ntp_feature);
  }
  return IsInUS();
}

std::string GetVariationsServiceCountryCode(
    variations::VariationsService* variations_service) {
  std::string country_code;
  if (!variations_service) {
    return country_code;
  }
  country_code = variations_service->GetStoredPermanentCountry();
  return country_code.empty() ? variations_service->GetLatestCountry()
                              : country_code;
}

bool IsTopSitesEnabled(Profile* profile) {
  return !IsCustomLinksEnabled(profile);
}

bool IsCustomLinksEnabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(ntp_prefs::kNtpCustomLinksVisible);
}

bool IsEnterpriseShortcutsEmpty(Profile* profile) {
  return profile->GetPrefs()
      ->GetList(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList)
      .empty();
}

bool IsEnterpriseShortcutsEnabled(Profile* profile) {
  // Enable enterprise shortcuts if the enterprise shortcuts policy is set, and
  // user has enabled visibility.
  return !IsEnterpriseShortcutsEmpty(profile) &&
         profile->GetPrefs()->GetBoolean(
             ntp_prefs::kNtpEnterpriseShortcutsVisible);
}

bool IsPersonalShortcutsVisible(Profile* profile) {
  // Always return true if no enterprise shortcuts are set by policy. Rely on
  // `IsTopSitesEnabled()` and `IsCustomLinksEnabled()` only.
  if (IsEnterpriseShortcutsEmpty(profile)) {
    return true;
  }
  return profile->GetPrefs()->GetBoolean(
      ntp_prefs::kNtpPersonalShortcutsVisible);
}

std::set<ntp_tiles::TileType> GetEnabledTileTypes(Profile* profile) {
  std::set<ntp_tiles::TileType> enabled_types;
  if (IsPersonalShortcutsVisible(profile) && IsCustomLinksEnabled(profile)) {
    enabled_types.insert(ntp_tiles::TileType::kCustomLinks);
  }
  if (IsPersonalShortcutsVisible(profile) && IsTopSitesEnabled(profile)) {
    enabled_types.insert(ntp_tiles::TileType::kTopSites);
  }
  if (IsEnterpriseShortcutsEnabled(profile)) {
    enabled_types.insert(ntp_tiles::TileType::kEnterpriseShortcuts);
  }
  return enabled_types;
}

// Updates the staleness info for shortcuts
void UpdateShortcutsStaleness(Profile* profile) {
  // Do not update staleness if shortcuts auto removal is disabled.
  if (profile->GetPrefs()->GetBoolean(
          ntp_prefs::kNtpShortcutsAutoRemovalDisabled)) {
    RecordShortcutsAutoRemovalMetrics(profile, /*prev_count=*/0);
    return;
  }

  // Do not update staleness if shortcuts are not visible.
  if (!profile->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible)) {
    RecordShortcutsAutoRemovalMetrics(profile, /*prev_count=*/0);
    return;
  }

  // Update the last update time if it is null.
  base::Time prev_update_time =
      profile->GetPrefs()->GetTime(ntp_prefs::kNtpLastShortcutsStalenessUpdate);
  if (prev_update_time.is_null()) {
    profile->GetPrefs()->SetTime(ntp_prefs::kNtpLastShortcutsStalenessUpdate,
                                 base::Time::Now());
    return;
  }

  // Update the staleness info if time delta is above the threshold.
  const base::Time shortcuts_load_time = base::Time::Now();
  const base::TimeDelta time_since_last_update =
      shortcuts_load_time - prev_update_time;
  const base::TimeDelta staleness_threshold =
      ntp_features::kShortcutsMinStalenessUpdateTimeInterval.Get();
  if (time_since_last_update <= staleness_threshold) {
    return;
  }

  const int staleness_count =
      profile->GetPrefs()->GetInteger(ntp_prefs::kNtpShortcutsStalenessCount);
  profile->GetPrefs()->SetTime(ntp_prefs::kNtpLastShortcutsStalenessUpdate,
                               shortcuts_load_time);
  profile->GetPrefs()->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount,
                                  staleness_count + 1);

  RecordShortcutsAutoRemovalMetrics(profile, staleness_count);
}

void DisableShortcutsAutoRemoval(Profile* profile) {
  profile->GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                  true);
}

void RecordShortcutsAutoRemovalMetrics(Profile* profile, int prev_count) {
  // Auto-removal skipped due to managed preference.
  if (IsEnterpriseShortcutsEnabled(profile)) {
    base::UmaHistogramEnumeration(
        kShortcutsAutoRemovalReasonHistogram,
        NtpShortcutsAutoRemovalReason::kManagedPreference);
    return;
  }

  // Auto-removal skipped due to shortcuts being hidden.
  if (!profile->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible)) {
    base::UmaHistogramEnumeration(kShortcutsAutoRemovalReasonHistogram,
                                  NtpShortcutsAutoRemovalReason::kNotVisible);
    return;
  }

  // Auto-removal skipped due to it being disabled.
  if (profile->GetPrefs()->GetBoolean(
          ntp_prefs::kNtpShortcutsAutoRemovalDisabled)) {
    base::UmaHistogramEnumeration(kShortcutsAutoRemovalReasonHistogram,
                                  NtpShortcutsAutoRemovalReason::kDisabled);
    return;
  }

  // Log the new staleness count for shortcuts. We're only logging it here
  // because the auto-removal will be skipped anyway due to conditions above.
  // NOTE: An exclusive max of 101 days was picked as the max threshold.
  base::UmaHistogramExactLinear("NewTabPage.MostVisited.AutoRemovalStaleDays",
                                prev_count, 101);

  // Auto-removal skipped due to the staleness threshold.
  if (prev_count < ntp_features::kStaleShortcutsCountThreshold.Get()) {
    base::UmaHistogramEnumeration(
        kShortcutsAutoRemovalReasonHistogram,
        NtpShortcutsAutoRemovalReason::kStaleDaysCount);
    return;
  }
}
