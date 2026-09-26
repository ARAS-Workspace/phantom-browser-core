// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_and_sync_status_metrics_provider.h"

#include <vector>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

ChromeSigninAndSyncStatusMetricsProvider::
    ChromeSigninAndSyncStatusMetricsProvider() = default;
ChromeSigninAndSyncStatusMetricsProvider::
    ~ChromeSigninAndSyncStatusMetricsProvider() = default;

void ChromeSigninAndSyncStatusMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  EmitHistograms(GetStatusOfAllProfiles());
}

signin_metrics::ProfilesStatus
ChromeSigninAndSyncStatusMetricsProvider::GetStatusOfAllProfiles() const {
  signin_metrics::ProfilesStatus profiles_status;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profile_list = profile_manager->GetLoadedProfiles();
  for (Profile* profile : profile_list) {
    auto* browser_collection = ProfileBrowserCollection::GetForProfile(profile);
    if (!browser_collection || browser_collection->GetSize() == 0) {
      // The profile is loaded, but there's no opened browser for this profile.
      continue;
    }

    auto* session_duration =
        metrics::DesktopProfileSessionDurationsServiceFactory::
            GetForBrowserContext(profile);
    // |session_duration| will be null for system and guest profiles.
    if (!session_duration) {
      continue;
    }
    UpdateProfilesStatusBasedOnSignInAndSyncStatus(
        profiles_status, session_duration->GetSigninStatus(),
        session_duration->IsSyncing());
  }
  return profiles_status;
}
