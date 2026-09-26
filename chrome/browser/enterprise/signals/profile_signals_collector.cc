// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/profile_signals_collector.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/policy/chrome_policy_blocklist_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace device_signals {

namespace {

bool GetBuiltInDnsClientEnabled(PrefService* local_state) {
  DCHECK(local_state);
  return local_state->GetBoolean(prefs::kBuiltInDnsClientEnabled);
}

}  // namespace

ProfileSignalsCollector::ProfileSignalsCollector(Profile* profile)
    : BaseSignalsCollector({
          {SignalName::kBrowserContextSignals,
           base::BindRepeating(&ProfileSignalsCollector::GetProfileSignals,
                               base::Unretained(this))},
      }),
      policy_blocklist_service_(
          ChromePolicyBlocklistServiceFactory::GetForProfile(profile)),
      profile_prefs_(profile->GetPrefs()),
      policy_manager_(profile->GetCloudPolicyManager()),
      profile_id_service_(
          enterprise::ProfileIdServiceFactory::GetForProfile(profile)) {
  CHECK(policy_blocklist_service_);
  CHECK(profile_id_service_);
}

ProfileSignalsCollector::~ProfileSignalsCollector() = default;

void ProfileSignalsCollector::GetProfileSignals(
    UserPermission permission,
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  ProfileSignalsResponse signal_response;
  signal_response.built_in_dns_client_enabled =
      GetBuiltInDnsClientEnabled(g_browser_process->local_state());
  signal_response.chrome_remote_desktop_app_blocked =
      device_signals::GetChromeRemoteDesktopAppBlocked(
          policy_blocklist_service_);
  signal_response.password_protection_warning_trigger =
      device_signals::GetPasswordProtectionWarningTrigger(profile_prefs_);
  signal_response.profile_enrollment_domain =
      device_signals::TryGetEnrollmentDomain(policy_manager_);
  signal_response.safe_browsing_protection_level =
      device_signals::GetSafeBrowsingProtectionLevel(profile_prefs_);
  signal_response.site_isolation_enabled =
      device_signals::GetSiteIsolationEnabled();
  signal_response.profile_id = profile_id_service_->GetProfileId();

  response.profile_signals_response = std::move(signal_response);

  // All signals are fetched synchronously for now, so we can run the closure
  // immediately. Once async signals are added, `done_closure` should be moved
  // to be run in the callback.
  std::move(done_closure).Run();
}

}  // namespace device_signals
