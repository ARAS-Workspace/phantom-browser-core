// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DATA_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "components/search_engines/template_url_data.h"

class Profile;

namespace search_engines {

// Profile specific data related to the search engine choice.
// `timestamp` is the search engine choice timestamp that's saved in the
// `kDefaultSearchProviderChoiceScreenCompletionTimestamp` pref.
// `chrome_version` is the Chrome version when the user made the choice.
// `default_search_engine` is the profile's default search engine.
struct ChoiceData {
  int64_t timestamp = 0;
  std::string chrome_version;
  TemplateURLData default_search_engine;
};

// LINT.IfChange(CurrentDefaultPropagationOutcome)
enum class CurrentDefaultPropagationOutcome {
  kPropagatedCurrentDefault = 0,
  kSkippedDueToPolicies = 1,
  kSkippedIsFallback = 2,
  kSkippedIsExtension = 3,
  kMaxValue = kSkippedIsExtension,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/search/enums.xml:CurrentDefaultPropagationOutcome)

// Returns a copy of the `ChoiceData` specific to `profile`, or `std::nullopt`
// if there is no default search engine to propagate. This function ignores
// any extension-provided default search engine to capture the underlying user
// choice (or system default), and skips propagation entirely if the default
// search is managed by an enterprise policy.
std::optional<ChoiceData> GetChoiceDataFromProfile(Profile& profile);

// Updates `profile` with the values from `choice_data`.
void UpdateProfileFromChoiceData(Profile& profile,
                                 const std::optional<ChoiceData>& choice_data);

}  // namespace search_engines

#endif  // CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DATA_H_
