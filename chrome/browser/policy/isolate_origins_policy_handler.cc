// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/isolate_origins_policy_handler.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"

#include "base/byte_size.h"
#include "base/system/sys_info.h"

#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/site_isolation/features.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

bool ValidatePolicyType(const PolicyMap& policies,
                        const char* policy_name,
                        base::Value::Type expected_type,
                        PolicyErrorMap* errors) {
  const base::Value* value = policies.GetValueUnsafe(policy_name);
  if (value && value->type() != expected_type) {
    if (errors) {
      errors->AddError(policy_name, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(expected_type));
    }
    return false;
  }
  return true;
}

}  // namespace

IsolateOriginsPolicyHandler::IsolateOriginsPolicyHandler() = default;
IsolateOriginsPolicyHandler::~IsolateOriginsPolicyHandler() = default;

bool IsolateOriginsPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                      PolicyErrorMap* errors) {
  std::vector<const char*> policies_to_check;

  policies_to_check.push_back(key::kIsolateOrigins);

  for (const char* policy_name : policies_to_check) {
    if (!ValidatePolicyType(policies, policy_name,
                            base::Value::Type::STRING, errors)) {
      return false;
    }
  }

  return true;
}

void IsolateOriginsPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  const base::Value* value_to_use = nullptr;

  value_to_use =
      policies.GetValue(key::kIsolateOrigins, base::Value::Type::STRING);

  if (value_to_use) {
    prefs->SetString(prefs::kIsolateOrigins, value_to_use->GetString());
  }
}

}  // namespace policy
