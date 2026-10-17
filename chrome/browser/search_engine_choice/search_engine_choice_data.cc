// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_data.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

namespace search_engines {

std::optional<ChoiceData> GetChoiceDataFromProfile(Profile& profile) {
  PrefService* pref_service = profile.GetPrefs();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile);
  CHECK(template_url_service);

  switch (template_url_service->default_search_provider_source()) {
    case DefaultSearchManager::FROM_FALLBACK:
      base::UmaHistogramEnumeration(
          "Search.ChoiceDebug.PropagatedDataOutcome",
          CurrentDefaultPropagationOutcome::kSkippedIsFallback);
      return std::nullopt;

    case DefaultSearchManager::FROM_EXTENSION:
      base::UmaHistogramEnumeration(
          "Search.ChoiceDebug.PropagatedDataOutcome",
          CurrentDefaultPropagationOutcome::kSkippedIsExtension);
      return std::nullopt;

    case DefaultSearchManager::FROM_POLICY:
    case DefaultSearchManager::FROM_POLICY_RECOMMENDED:
      base::UmaHistogramEnumeration(
          "Search.ChoiceDebug.PropagatedDataOutcome",
          CurrentDefaultPropagationOutcome::kSkippedDueToPolicies);
      return std::nullopt;

    case DefaultSearchManager::FROM_USER:
      break;  // Current default eligible for propagation.
  }

  CHECK(template_url_service->GetDefaultSearchProvider());
  const TemplateURLData& default_search_engine =
      template_url_service->GetDefaultSearchProvider()->data();

  base::UmaHistogramEnumeration(
      "Search.ChoiceDebug.PropagatedDataOutcome",
      CurrentDefaultPropagationOutcome::kPropagatedCurrentDefault);

  return search_engines::ChoiceData{
      .timestamp = pref_service->GetInt64(
          prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
      .chrome_version = pref_service->GetString(
          prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
      .default_search_engine = default_search_engine};
}

void UpdateProfileFromChoiceData(Profile& profile,
                                 const std::optional<ChoiceData>& choice_data) {
  if (!choice_data.has_value()) {
    return;
  }

  const TemplateURLData& default_search_engine =
      choice_data->default_search_engine;

  PrefService* pref_service = profile.GetPrefs();
  if (choice_data->timestamp != 0) {
    pref_service->SetInt64(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
        choice_data->timestamp);
  }

  if (!choice_data->chrome_version.empty()) {
    pref_service->SetString(
        prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
        choice_data->chrome_version);
  }

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile);
  CHECK(template_url_service);
  TemplateURL template_url(default_search_engine);
  template_url_service->SetUserSelectedDefaultSearchProvider(&template_url);
}

}  // namespace search_engines
