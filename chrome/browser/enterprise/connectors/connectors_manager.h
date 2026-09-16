// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/connectors_manager_base.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class GlobalBrowserCollection;

namespace storage {
class FileSystemURL;
}

namespace enterprise_connectors {

// This class overrides `ConnectorsManagerBase` for desktop and Android usage.
// It manages access to Reporting and Analysis Connector policies for a given
// profile.
class ConnectorsManager : public ConnectorsManagerBase {

 public:
  using ConnectorsManagerBase::AnalysisConnectorsSettings;
  using ConnectorsManagerBase::GetAnalysisSettings;

  ConnectorsManager(PrefService* pref_service,
                    const ServiceProviderConfig* config,
                    bool observe_prefs = true);
  ~ConnectorsManager() override;

 private:

  void CacheAnalysisConnectorPolicy(AnalysisConnector connector) const override;

  // Get data location region from policy.
  DataRegion GetDataRegion(AnalysisConnector connector) const override;

  // Re-cache analysis connector policy and update local agent connection if
  // needed.
  void OnAnalysisPrefChanged(AnalysisConnector connector) override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
