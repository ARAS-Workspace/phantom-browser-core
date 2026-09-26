// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_browser_sampling_trials.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/common/channel_info.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "components/version_info/channel.h"

namespace metrics {
namespace {

// Note that the trial name must be kept in sync with the server config
// controlling sampling. If they don't match, then clients will be shuffled into
// different groups when the server config takes over from the fallback trial.
constexpr char kUkmSamplingTrialName[] = "UkmSamplingRate";

// Unconditionally attempts to create a field trial to control client side
// UKM sampling to use as a fallback when one hasn't been provided. This is
// expected to occur on first-run on platforms that don't have first-run
// variations support. This should only be called when there is no existing
// field trial controlling the sampling feature.
void CreateFallbackUkmSamplingTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    bool is_stable_channel,
    base::FeatureList* feature_list) {
  static const char kSampledGroup_Stable[] = "Sampled_NoSeed_Stable";
  static const char kSampledGroup_Other[] = "Sampled_NoSeed_Other";
  const char* sampled_group = kSampledGroup_Other;
  int default_sampling = 1;  // Sampling is 1-in-N; this is N.

  // Nothing is sampled out except for "stable" which omits almost everything
  // in this configuration. This is done so that clients that fail to receive
  // a configuration from the server do not bias aggregated results because
  // of a relatively large number of records from them.
  if (is_stable_channel) {
    sampled_group = kSampledGroup_Stable;
    default_sampling = 1000000;
  }

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kUkmSamplingTrialName, /*total_probability=*/100, sampled_group,
          entropy_provider));

  // Everybody (100%) should have a sampling configuration.
  std::map<std::string, std::string> params = {
      {"_default_sampling", base::NumberToString(default_sampling)}};
  base::AssociateFieldTrialParams(trial->trial_name(), sampled_group,
                                  std::move(params));
  trial->AppendGroup(sampled_group, 100);

  // Setup the feature.
  feature_list->RegisterFieldTrialOverride(
      ukm::kUkmSamplingRateFeature.name,
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
}

}  // namespace

void CreateFallbackSamplingTrialsIfNeeded(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    base::FeatureList* feature_list) {
}

void CreateFallbackUkmSamplingTrialIfNeeded(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    base::FeatureList* feature_list) {
  if (!base::FieldTrialList::TrialExists(kUkmSamplingTrialName)) {
    const bool is_stable =
        chrome::GetChannel() == version_info::Channel::STABLE;
    CreateFallbackUkmSamplingTrial(entropy_provider, is_stable, feature_list);
  }
}

}  // namespace metrics
