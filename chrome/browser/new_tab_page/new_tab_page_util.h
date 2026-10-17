// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_

#include <set>

#include "base/feature_list.h"
#include "base/values.h"
#include "components/ntp_tiles/tile_type.h"

class Profile;

namespace variations {
class VariationsService;
}  // namespace variations

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(NtpShortcutsAutoRemovalReason)
enum class NtpShortcutsAutoRemovalReason {
  kManagedPreference = 0,
  kNotVisible = 1,
  kDisabled = 2,
  kStaleDaysCount = 3,
  kMaxValue = kStaleDaysCount,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:NtpShortcutsAutoRemovalReason)

bool IsEnUSLocaleOnlyFeatureEnabled(const base::Feature& ntp_feature);

// Return the country code as provided by the variations service.
std::string GetVariationsServiceCountryCode(
    variations::VariationsService* variations_service);

bool IsTopSitesEnabled(Profile* profile);
bool IsCustomLinksEnabled(Profile* profile);
bool IsEnterpriseShortcutsEnabled(Profile* profile);
bool IsPersonalShortcutsVisible(Profile* profile);
// Returns the set of enabled NTP tile types.
std::set<ntp_tiles::TileType> GetEnabledTileTypes(Profile* profile);

// Updates the staleness information for shortcuts.
void UpdateShortcutsStaleness(Profile* profile);

// Sets the kNtpShortcutsAutoRemovalDisabled pref to true for the module.
void DisableShortcutsAutoRemoval(Profile* profile);

// Logs the auto-removal metrics for shortcuts, including:
// (1) The reason the shortcuts were not auto-removed
// (2) The new staleness count for the shortcuts if applicable
void RecordShortcutsAutoRemovalMetrics(Profile* profile, int prev_count);

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_
