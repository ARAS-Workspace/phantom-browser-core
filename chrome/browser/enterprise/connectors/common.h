// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/supports_user_data.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/global_routing_id.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

namespace policy {
class BrowserPolicyConnector;
}  // namespace policy

namespace enterprise_connectors {

// User data to persist a save package's final callback allowing/denying
// completion. This is used since the callback can be called either when
// scanning completes on a block/allow verdict, when the user cancels the scan,
// or when the user bypasses scanning.
struct SavePackageScanningData : public base::SupportsUserData::Data {
  explicit SavePackageScanningData(
      content::SavePackageAllowedCallback callback);
  ~SavePackageScanningData() override;
  static const char kKey[];

  content::SavePackageAllowedCallback callback;
};

policy::BrowserPolicyConnector* GetBrowserPolicyConnector();

// Checks `item` for a SavePackageScanningData, and run it's callback with
// `allowed` if there is one.
void RunSavePackageScanningCallback(download::DownloadItem* item, bool allowed);

// Returns whether the profile is affiliated. This will only return true if both
// the device and profile are managed, and if both share affiliation IDs.
bool IsAffiliated(Profile* profile);

// Returns whether device info should be reported for the profile.
bool IncludeDeviceInfo(Profile* profile, bool per_profile);

// Returns the email address of the unconsented account signed in to the profile
// or an empty string if no account is signed in.  If `profile` is null then the
// empty string is returned.
std::string GetProfileEmail(Profile* profile);

// Returns the list of URLs from the current frame all the way to the outermost
// frame URL. Above the `kMaxFrameUrls` limit, we skip the rest of the chain and
// take the outermost URL for performance considerations.
//
// The chain is collected starting from `initiating_frame_id` if provided.
// If the frame ID is provided but the frame is dead, it returns an empty chain.
google::protobuf::RepeatedPtrField<std::string> CollectFrameUrls(
    content::WebContents* web_contents,
    DeepScanAccessPoint access_point,
    std::optional<content::GlobalRenderFrameHostId> initiating_frame_id =
        std::nullopt);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
