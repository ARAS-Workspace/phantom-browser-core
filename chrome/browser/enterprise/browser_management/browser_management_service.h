// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_

#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace policy {

// This class gives information related to the browser's management state.
// For more information please read
// //components/policy/core/common/management/management_service.md
class BrowserManagementService : public ManagementService, public KeyedService {
 public:
  explicit BrowserManagementService(Profile* profile);
  ~BrowserManagementService() override;

  void TriggerPolicyStatusChangedForTesting() override;

 private:
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Starts listening to changes to policies that affect the enterprise label
  // and pill.
  void StartListeningToPrefChanges(Profile* profile);

  // Sets the enterprise label in the profile attribute storage and notifies
  // observers of the change.
  void UpdateEnterpriseLabelForProfile(Profile* profile);

  PrefChangeRegistrar pref_change_registrar_;
  base::WeakPtrFactory<BrowserManagementService> weak_ptr_factory_{this};
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_
