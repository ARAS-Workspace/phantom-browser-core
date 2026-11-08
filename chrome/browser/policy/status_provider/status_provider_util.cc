// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/status_provider_util.h"

#include "base/values.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "google_apis/gaia/gaia_auth_util.h"

const char kDevicePolicyStatusDescription[] = "statusDevice";
const char kUserPolicyStatusDescription[] = "statusUser";

void SetDomainExtractedFromUsername(base::DictValue& dict) {

  const std::string* username = dict.FindString(policy::kUsernameKey);
  if (username && !username->empty())
    dict.Set(policy::kDomainKey, gaia::ExtractDomainName(*username));
}

void SetDomainExtractedFromUsername(policy::mojom::StatusPtr& status) {

  const auto& username = status->username;
  if (username.has_value() && !username->empty()) {
    status->domain = gaia::ExtractDomainName(username.value());
  }
}

void GetUserAffiliationStatus(base::DictValue* dict, Profile* profile) {
  CHECK(profile);
}

void SetProfileId(base::DictValue* dict, Profile* profile) {
  CHECK(profile);
}
