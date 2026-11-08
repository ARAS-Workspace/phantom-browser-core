// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/policy_status_provider.h"

#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

class MockPolicyStatusObserver : public PolicyStatusProvider::Observer {
 public:
  MOCK_METHOD(void, OnPolicyStatusChanged, (), (override));
};

class TestPolicyStatusProvider : public PolicyStatusProvider {
 public:
  TestPolicyStatusProvider() = default;
  ~TestPolicyStatusProvider() override = default;

  void TriggerNotifyStatusChange() { NotifyStatusChange(); }
};

TEST(PolicyStatusProviderTest, DefaultStatus) {
  PolicyStatusProvider provider;
  EXPECT_TRUE(provider.GetStatus().empty());
}

TEST(PolicyStatusProviderTest, ObserverNotification) {
  TestPolicyStatusProvider provider;
  testing::StrictMock<MockPolicyStatusObserver> observer;

  provider.AddObserver(&observer);
  EXPECT_CALL(observer, OnPolicyStatusChanged()).Times(1);
  provider.TriggerNotifyStatusChange();
  testing::Mock::VerifyAndClearExpectations(&observer);

  provider.RemoveObserver(&observer);
  EXPECT_CALL(observer, OnPolicyStatusChanged()).Times(0);
  provider.TriggerNotifyStatusChange();
}

}  // namespace
}  // namespace policy
