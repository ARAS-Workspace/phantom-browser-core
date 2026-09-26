// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/tips_client.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/notifications/scheduler/public/tips_agent.h"
#include "chrome/browser/tips/core/tips_prefs.h"
#include "chrome/browser/tips/core/tips_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

class MockTipsAgent : public TipsAgent {
 public:
  MOCK_METHOD(void,
              ShowTipsPromo,
              (tips::TipsNotificationsFeatureType feature_type),
              (override));
};

}  // namespace

class TipsClientTest : public testing::Test {
 public:
  TipsClientTest() {}

 public:
  void SetUp() override {
    std::unique_ptr<TipsAgent> mock_tips_agent =
        std::make_unique<MockTipsAgent>();
    mock_tips_agent_ = static_cast<MockTipsAgent*>(mock_tips_agent.get());
    tips_client_ = std::make_unique<TipsClient>(std::move(mock_tips_agent),
                                                &pref_service_);
  }

 protected:
  NotificationSchedulerClient* tips_client() { return tips_client_.get(); }
  MockTipsAgent* mock_tips_agent() { return mock_tips_agent_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  std::unique_ptr<TipsClient> tips_client_;
  raw_ptr<MockTipsAgent> mock_tips_agent_;
  TestingPrefServiceSimple pref_service_;
};

}  // namespace notifications
