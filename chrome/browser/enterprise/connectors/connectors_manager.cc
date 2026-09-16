// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_manager.h"

#include <memory>

#include "base/check.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

}  // namespace

ConnectorsManager::ConnectorsManager(PrefService* pref_service,
                                     const ServiceProviderConfig* config,
                                     bool observe_prefs)
    : ConnectorsManagerBase(pref_service, config, observe_prefs) {

  if (observe_prefs) {
    StartObservingPrefs(pref_service);
  }
}

ConnectorsManager::~ConnectorsManager() = default;

void ConnectorsManager::CacheAnalysisConnectorPolicy(
    AnalysisConnector connector) const {
  analysis_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = AnalysisConnectorPref(connector);
  DCHECK(pref);

  const base::ListValue& policy_value = prefs()->GetList(pref);
  for (const base::Value& service_settings : policy_value) {
    analysis_connector_settings_[connector].push_back(
        std::make_unique<AnalysisServiceSettings>(service_settings,
                                                  *service_provider_config_));
  }
}

void ConnectorsManager::OnAnalysisPrefChanged(AnalysisConnector connector) {
  CacheAnalysisConnectorPolicy(connector);
}

DataRegion ConnectorsManager::GetDataRegion(AnalysisConnector connector) const {
#if BUILDFLAG(IS_ANDROID)
  return DataRegion::NO_PREFERENCE;
#else
  // Connector's policy scope determines the DRZ policy scope to use.
  policy::PolicyScope scope = static_cast<policy::PolicyScope>(
      prefs()->GetInteger(AnalysisConnectorScopePref(connector)));

  const PrefService* pref_service =
      (scope == policy::PolicyScope::POLICY_SCOPE_MACHINE)
          ? g_browser_process->local_state()
          : prefs();

  if (!pref_service ||
      !pref_service->HasPrefPath(prefs::kChromeDataRegionSetting)) {
    return DataRegion::NO_PREFERENCE;
  }

  return ChromeDataRegionSettingToEnum(
      pref_service->GetInteger(prefs::kChromeDataRegionSetting));
#endif
}

}  // namespace enterprise_connectors
