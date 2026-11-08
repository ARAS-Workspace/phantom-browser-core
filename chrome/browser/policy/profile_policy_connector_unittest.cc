// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/local_test_policy_provider.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

// Waits for a PolicyService to notify its observers that initialization of a
// PolicyDomain has finished.
class PolicyServiceInitializedWaiter : PolicyService::Observer {
 public:
  // Instantiates a PolicyServiceInitializedWaiter which will wait for
  // |policy_service| to signal that |policy_domain| has completed
  // initialization. |policy_service| must outlive this object.
  PolicyServiceInitializedWaiter(PolicyService* policy_service,
                                 PolicyDomain policy_domain)
      : policy_service_(policy_service), policy_domain_(policy_domain) {
    policy_service_->AddObserver(policy_domain_, this);
  }
  PolicyServiceInitializedWaiter(const PolicyServiceInitializedWaiter&) =
      delete;
  PolicyServiceInitializedWaiter& operator=(
      const PolicyServiceInitializedWaiter&) = delete;

  ~PolicyServiceInitializedWaiter() override {
    policy_service_->RemoveObserver(policy_domain_, this);
  }

  // Waits for the PolicyService to signal that the PolicyDomain has completed
  // initialization. If initialization of the PolicyDomain is already complete
  // at the time Wait() is called, returns immediately.
  void Wait() {
    if (policy_service_->IsInitializationComplete(policy_domain_)) {
      return;
    }
    run_loop_.Run();
  }

  // PolicyService::Observer:
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override {
    run_loop_.Quit();
  }

  // PolicyService::Observer:
  void OnPolicyServiceInitialized(PolicyDomain domain) override {
    run_loop_.Quit();
  }

  base::RunLoop* run_loop() { return &run_loop_; }

 private:
  raw_ptr<PolicyService> policy_service_;
  PolicyDomain policy_domain_;
  base::RunLoop run_loop_;
};

}  // namespace

class ProfilePolicyConnectorTest : public testing::Test {
 protected:
  ProfilePolicyConnectorTest() = default;
  ~ProfilePolicyConnectorTest() override = default;

  void TearDown() override {
    task_environment_.RunUntilIdle();

    // Some tests override the policy service via this global singleton. Unset
    // it here to make sure the cleanup happens.
    BrowserPolicyConnectorBase::SetPolicyServiceForTesting(nullptr);
  }

  // Needs to be the first member. Some tests need to be
  // called on the called on Chrome_UIThread.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  SchemaRegistry schema_registry_;
};

TEST_F(ProfilePolicyConnectorTest, LocalTestProviderUseAndRevert) {
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());

  // Set up connector
  std::unique_ptr<LocalTestPolicyProvider> local_test_policy_provider =
      LocalTestPolicyProvider::CreateIfAllowed(version_info::Channel::DEFAULT);
  g_browser_process->browser_policy_connector()
      ->SetLocalTestPolicyProviderForTesting(local_test_policy_provider.get());

  ProfilePolicyConnector connector;
  connector.Init(/*user=*/nullptr, &schema_registry_,
                 g_browser_process->browser_policy_connector(),
                 /*force_immediate_load=*/false);

  // Set policy to local test policy provider.
  local_test_policy_provider->LoadJsonPolicies(R"([
      {"level": 1,"scope": 0,"source": 2,
      "name": "CloudReportingEnabled","value": false}
      ])");

  // Set Local Testing to active.
  connector.UseLocalTestPolicyProvider();
  PolicyServiceInitializedWaiter(connector.policy_service(),
                                 POLICY_DOMAIN_CHROME)
      .run_loop()
      ->Run();

  // Verify local testing policies is applied.
  const base::Value* value =
      connector.policy_service()
          ->GetPolicies(chrome_namespace)
          .GetValue(key::kCloudReportingEnabled, base::Value::Type::BOOLEAN);
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value(false), *value);

  // Revert Local Testing
  connector.RevertUseLocalTestPolicyProvider();
  PolicyServiceInitializedWaiter(connector.policy_service(),
                                 POLICY_DOMAIN_CHROME)
      .run_loop()
      ->Run();

  // Verify local test provider is inactive
  ASSERT_FALSE(
      connector.policy_service()
          ->GetPolicies(chrome_namespace)
          .GetValue(key::kCloudReportingEnabled, base::Value::Type::BOOLEAN));

  // Cleanup.
  g_browser_process->browser_policy_connector()
      ->SetLocalTestPolicyProviderForTesting(nullptr);
  connector.Shutdown();
}

}  // namespace policy
