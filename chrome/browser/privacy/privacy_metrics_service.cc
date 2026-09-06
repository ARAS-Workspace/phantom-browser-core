// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/privacy_metrics_service.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/spellcheck_buildflags.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/browser/pref_names.h"
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

PrivacyMetricsService::PrivacyMetricsService(
    PrefService* pref_service,
    HostContentSettingsMap* host_content_settings_map)
    : pref_service_(pref_service),
      host_content_settings_map_(host_content_settings_map) {
  DCHECK(pref_service_);
  DCHECK(host_content_settings_map_);

  RecordStartupMetrics();
}

PrivacyMetricsService::~PrivacyMetricsService() = default;

void PrivacyMetricsService::RecordStartupMetrics() {
  base::UmaHistogramBoolean(
      "Privacy.DoNotTrackSetting2",
      pref_service_->GetBoolean(prefs::kEnableDoNotTrack));

  base::UmaHistogramEnumeration("Settings.PreloadStatus.OnStartup3",
                                prefetch::GetPreloadPagesState(*pref_service_));
  base::UmaHistogramBoolean(
      "Settings.AutocompleteSearches.OnStartup2",
      pref_service_->GetBoolean(::prefs::kSearchSuggestEnabled));

#if BUILDFLAG(ENABLE_SPELLCHECK)
  base::UmaHistogramBoolean(
      "Settings.AdvancedSpellcheck.OnStartup2",
      pref_service_->GetBoolean(
          ::spellcheck::prefs::kSpellCheckUseSpellingService));
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
}
