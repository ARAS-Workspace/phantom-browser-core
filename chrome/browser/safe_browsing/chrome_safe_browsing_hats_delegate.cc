// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_hats_delegate.h"

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

ChromeSafeBrowsingHatsDelegate::ChromeSafeBrowsingHatsDelegate(Profile* profile)
    : profile_(profile) {}

void ChromeSafeBrowsingHatsDelegate::LaunchRedWarningSurvey(
    const SurveyStringData& product_specific_string_data,
    const SurveyBitsData& product_specific_bits_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile_ || profile_->IsOffTheRecord()) {
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }
  // `product_specific_string_data` contains a superset of fields relevant to
  // HaTS surveys on desktop, so extract only the relevant subset for desktop.
  SurveyStringData desktop_string_data;
  for (const char* key :
       {safe_browsing::kFlaggedUrl, safe_browsing::kMainFrameUrl,
        safe_browsing::kReferrerUrl, safe_browsing::kUserActivityWithUrls}) {
    if (auto it = product_specific_string_data.find(key);
        it != product_specific_string_data.end()) {
      desktop_string_data.emplace(key, it->second);
    }
  }
  // Desktop HaTS red warning surveys do not currently attach any product
  // specific bits data.
  SurveyBitsData desktop_bits_data;
  hats_service->LaunchSurvey(kHatsSurveyTriggerRedWarning,
                             /*success_callback=*/base::DoNothing(),
                             /*failure_callback=*/base::DoNothing(),
                             desktop_bits_data, desktop_string_data);
}

}  // namespace safe_browsing
