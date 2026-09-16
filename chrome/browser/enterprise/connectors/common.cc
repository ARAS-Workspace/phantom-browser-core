// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/common.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/enterprise/connectors/core/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "components/prefs/pref_service.h"
#endif

namespace enterprise_connectors {

namespace {

}  // namespace

policy::BrowserPolicyConnector* GetBrowserPolicyConnector() {
  return g_browser_process ? g_browser_process->browser_policy_connector()
                           : nullptr;
}

const char SavePackageScanningData::kKey[] =
    "enterprise_connectors.save_package_scanning_key";
SavePackageScanningData::SavePackageScanningData(
    content::SavePackageAllowedCallback callback)
    : callback(std::move(callback)) {}
SavePackageScanningData::~SavePackageScanningData() = default;

void RunSavePackageScanningCallback(download::DownloadItem* item,
                                    bool allowed) {
  DCHECK(item);

  auto* data = static_cast<SavePackageScanningData*>(
      item->GetUserData(SavePackageScanningData::kKey));
  if (data && !data->callback.is_null())
    std::move(data->callback).Run(allowed);
}

bool IsAffiliated(Profile* profile) {
  return enterprise_util::IsProfileAffiliated(profile);
}

bool IncludeDeviceInfo(Profile* profile, bool per_profile) {
  // A browser managed through the device can send device info.
  if (!per_profile) {
    return true;
  }

  // An unmanaged browser shouldn't share its device info for privacy reasons.
  if (!policy::GetDMToken(profile).is_valid()) {
    return false;
  }
  // A managed device can share its info with the profile if they are
  // affiliated.
  return IsAffiliated(profile);
}

std::string GetProfileEmail(Profile* profile) {
  if (!profile) {
    return std::string();
  }

  std::string email =
      GetProfileEmail(IdentityManagerFactory::GetForProfile(profile));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (email.empty()) {
    email = profile->GetPrefs()->GetString(
        enterprise_signin::prefs::kProfileUserEmail);
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  return email;
}

google::protobuf::RepeatedPtrField<std::string> CollectFrameUrls(
    content::WebContents* web_contents,
    DeepScanAccessPoint access_point,
    std::optional<content::GlobalRenderFrameHostId> initiating_frame_id) {
  return google::protobuf::RepeatedPtrField<std::string>();
}

}  // namespace enterprise_connectors
