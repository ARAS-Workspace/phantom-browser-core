// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/startup_data.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/prefs/profile_pref_store_manager.h"
#include "chrome/common/channel_info.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/entropy_state_provider.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/install_date_provider.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/metrics/version_utils.h"
#include "third_party/metrics_proto/system_profile.pb.h"

StartupData::StartupData() = default;

StartupData::~StartupData() = default;

// TODO(martinkong): Remove this function and replace its usage with
// ChromeFeatureListCreator::GetInstance()
ChromeFeatureListCreator* StartupData::chrome_feature_list_creator() {
  return ChromeFeatureListCreator::GetInstance();
}

void StartupData::RecordCoreSystemProfile() {
  metrics::SystemProfileProto system_profile;
  metrics::MetricsLog::RecordCoreSystemProfile(
      metrics::GetVersionString(),
      metrics::AsProtobufChannel(chrome::GetChannel()),
      chrome::IsExtendedStableChannel(),
      chrome_feature_list_creator()->actual_locale(),
      metrics::GetAppPackageName(), &system_profile);

  metrics::DelegatingProvider delegating_provider;

  // TODO(hanxi): Create SyntheticTrialRegistry and pass it to
  // |field_trial_provider|.
  delegating_provider.RegisterMetricsProvider(
      std::make_unique<variations::FieldTrialsProvider>(nullptr,
                                                        std::string_view()));

  // Persists low entropy source values.
  delegating_provider.RegisterMetricsProvider(
      std::make_unique<metrics::EntropyStateProvider>(
          chrome_feature_list_creator()->local_state()));

  // Register CPUMetricsProvider for hardware details.
  delegating_provider.RegisterMetricsProvider(
      std::make_unique<metrics::CPUMetricsProvider>());

  // Register InstallDateProvider.
  delegating_provider.RegisterMetricsProvider(
      std::make_unique<metrics::InstallDateProvider>(
          chrome_feature_list_creator()->local_state()));

  delegating_provider.ProvideSystemProfileMetricsWithLogCreationTime(
      base::TimeTicks(), &system_profile);

  metrics::GlobalPersistentSystemProfile::GetInstance()->SetSystemProfile(
      system_profile, /* complete */ false);
}
