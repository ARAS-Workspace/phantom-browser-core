// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kUrl1[] = "https://example1.com";
const char kUrl2[] = "https://example2.com";
const char kUrl3[] = "https://example3.com";

class MockSafetyHubNotificationWrapper
    : public RevokedPermissionsOSNotificationDisplayManager::
          SafetyHubNotificationWrapper {
 public:
  MockSafetyHubNotificationWrapper() = default;
  ~MockSafetyHubNotificationWrapper() override = default;

  MOCK_METHOD(void,
              DisplayNotification,
              (int num_revoked_permissions,
               std::string& first_affected_domain,
               bool any_suspicious_revocations,
               bool any_disruptive_revocations),
              (override));
  MOCK_METHOD(void,
              UpdateNotification,
              (int num_revoked_permissions,
               std::string& first_affected_domain,
               bool any_suspicious_revocations,
               bool any_disruptive_revocations),
              (override));
};

}  // namespace

class RevokedPermissionsOSNotificationDisplayManagerTest
    : public ::testing::Test {
 public:
  void SetUp() override {
    auto mock_wrapper = std::make_unique<MockSafetyHubNotificationWrapper>();
    mock_wrapper_ = mock_wrapper.get();
    manager_ = std::make_unique<RevokedPermissionsOSNotificationDisplayManager>(
        hcsm(), std::move(mock_wrapper));
  }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

  void AddDisruptiveRevocation(const GURL& url) {
    DisruptiveNotificationPermissionsManager::RevocationEntry entry(
        DisruptiveNotificationPermissionsManager::RevocationState::kRevoked,
        /*site_engagement=*/0.0,
        /*daily_notification_count=*/4,
        /*timestamp=*/base::Time::Now());
    DisruptiveNotificationPermissionsManager::ContentSettingHelper(*hcsm())
        .PersistRevocationEntry(url, entry);
  }

  TestingProfile* profile() { return &profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<RevokedPermissionsOSNotificationDisplayManager> manager_;
  raw_ptr<MockSafetyHubNotificationWrapper> mock_wrapper_;
  TestingProfile profile_;
};

TEST_F(RevokedPermissionsOSNotificationDisplayManagerTest,
       DisruptiveRevocationsCounted) {
  AddDisruptiveRevocation(GURL(kUrl1));
  AddDisruptiveRevocation(GURL(kUrl2));

  EXPECT_CALL(*mock_wrapper_,
              DisplayNotification(2, testing::_, testing::_, testing::_));
  manager_->DisplayNotification();
}

TEST_F(RevokedPermissionsOSNotificationDisplayManagerTest, UpdateNotification) {
  AddDisruptiveRevocation(GURL(kUrl2));

  EXPECT_CALL(*mock_wrapper_,
              DisplayNotification(1, testing::_, testing::_, testing::_));
  manager_->DisplayNotification();

  testing::Mock::VerifyAndClearExpectations(mock_wrapper_);

  AddDisruptiveRevocation(GURL(kUrl3));

  EXPECT_CALL(*mock_wrapper_,
              UpdateNotification(2, testing::_, testing::_, testing::_));
  manager_->UpdateNotification();
}
