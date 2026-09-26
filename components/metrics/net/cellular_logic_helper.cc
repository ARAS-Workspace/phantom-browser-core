// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/cellular_logic_helper.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/metrics_features.h"
#include "net/base/network_change_notifier.h"

namespace metrics {

namespace {

const int kStandardUploadIntervalSeconds = 30 * 60;  // Thirty minutes.

// This parameter is intended to be used for Structured metrics, which is
// currently only enabled on Chrome OS.
//
// This parameter should not be used for cellular devices.
constexpr base::FeatureParam<int> kUmaUploadCadence{
    &features::kStructuredMetrics, "uma_upload_cadence",
    kStandardUploadIntervalSeconds};

// Android-only cellular settings.

}  // namespace

base::TimeDelta GetUploadInterval(bool use_cellular_upload_interval) {
  return base::Seconds(kUmaUploadCadence.Get());
}

bool ShouldUseCellularUploadInterval() {
  return false;
}

}  // namespace metrics
