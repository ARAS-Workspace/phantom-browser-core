// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_dir_policy_handler.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace {

const char* kUserIDHash = "deadbeef";

}  // namespace

class DownloadDirPolicyHandlerTest
    : public policy::ConfigurationPolicyPrefStoreTest {
 public:
  void SetUp() override {
    recommended_store_ = new policy::ConfigurationPolicyPrefStore(
        nullptr, policy_service_.get(), &handler_list_,
        policy::POLICY_LEVEL_RECOMMENDED);
    handler_list_.AddHandler(
        base::WrapUnique<policy::ConfigurationPolicyHandler>(
            new DownloadDirPolicyHandler));
  }

  void PopulatePolicyHandlerParameters(
      policy::PolicyHandlerParameters* parameters) override {
    parameters->user_id_hash = kUserIDHash;
  }

 protected:
  scoped_refptr<policy::ConfigurationPolicyPrefStore> recommended_store_;
};

TEST_F(DownloadDirPolicyHandlerTest, SetDownloadDirectory) {
  policy::PolicyMap policy;
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, nullptr));
  policy.Set(policy::key::kDownloadDirectory, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(std::string()), nullptr);
  UpdateProviderPolicy(policy);

  // Setting a DownloadDirectory should disable the PromptForDownload pref.
  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(prefs::kPromptForDownload, &value));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_bool());
  EXPECT_FALSE(value->GetBool());
}
