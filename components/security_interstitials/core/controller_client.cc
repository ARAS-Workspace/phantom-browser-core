// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/controller_client.h"

#include <utility>

#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

const char kBoxChecked[] = "boxchecked";
const char kDisplayCheckBox[] = "displaycheckbox";
const char kOptInLink[] = "optInLink";

ControllerClient::ControllerClient(
    std::unique_ptr<MetricsHelper> metrics_helper)
    : metrics_helper_(std::move(metrics_helper)) {}

ControllerClient::~ControllerClient() = default;

MetricsHelper* ControllerClient::metrics_helper() const {
  return metrics_helper_.get();
}

void ControllerClient::SetReportingPreference(bool report) {
  DCHECK(GetPrefService());
  GetPrefService()->SetBoolean(GetExtendedReportingPrefName(), report);
  metrics_helper_->RecordUserInteraction(
      report ? MetricsHelper::SET_EXTENDED_REPORTING_ENABLED
             : MetricsHelper::SET_EXTENDED_REPORTING_DISABLED);
}

bool ControllerClient::HasSeenRecurrentError() {
  return false;
}

}  // namespace security_interstitials
