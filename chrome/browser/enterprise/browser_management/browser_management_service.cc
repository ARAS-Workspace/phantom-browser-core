// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/browser_management_service.h"

#include <algorithm>

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/browser_management_status_provider.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace policy {

namespace {

std::vector<std::unique_ptr<ManagementStatusProvider>>
GetManagementStatusProviders(Profile* profile) {
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.emplace_back(
      std::make_unique<LocalBrowserManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<LocalDomainBrowserManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<ProfileCloudManagementStatusProvider>(profile));
  providers.emplace_back(
      std::make_unique<LocalTestPolicyUserManagementProvider>(profile));
  providers.emplace_back(
      std::make_unique<LocalTestPolicyBrowserManagementProvider>(profile));
  return providers;
}

}  // namespace

BrowserManagementService::BrowserManagementService(Profile* profile)
    : ManagementService(GetManagementStatusProviders(profile)) {
  // Ensure no profile management status provider requires a cache.
  // If we want to support async providers in `BrowserManagementService`,
  // we can use `ScopedProfileKeepAlive` to make sure the provider is not
  // deleted while it is running.
  CHECK(std::none_of(
      management_status_providers().begin(),
      management_status_providers().end(),
      [](const auto& provider) { return provider->RequiresCache(); }));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  UpdateEnterpriseLabelForProfile(profile);
  StartListeningToPrefChanges(profile);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

void BrowserManagementService::TriggerPolicyStatusChangedForTesting() {
  CHECK_IS_TEST();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  NotifyEnterpriseLabelUpdated();
#endif
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void BrowserManagementService::StartListeningToPrefChanges(Profile* profile) {
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kEnterpriseCustomLabelForProfile,
      base::BindRepeating(
          &BrowserManagementService::UpdateEnterpriseLabelForProfile,
          weak_ptr_factory_.GetWeakPtr(), profile));
  pref_change_registrar_.Add(
      prefs::kEnterpriseProfileBadgeToolbarSettings,
      base::BindRepeating(
          &BrowserManagementService::UpdateEnterpriseLabelForProfile,
          weak_ptr_factory_.GetWeakPtr(), profile));
}

void BrowserManagementService::UpdateEnterpriseLabelForProfile(
    Profile* profile) {
  enterprise_util::SetEnterpriseProfileLabel(profile);
  NotifyEnterpriseLabelUpdated();
}

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

BrowserManagementService::~BrowserManagementService() = default;

}  // namespace policy
