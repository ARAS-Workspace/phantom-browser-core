// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/safe_browsing/advanced_protection_status_manager_desktop.h"

namespace safe_browsing {

typedef AdvancedProtectionStatusManagerDesktop
    AdvancedProtectionStatusManagerPlatform;

class AdvancedProtectionStatusManagerTest : public testing::Test {
 public:
  AdvancedProtectionStatusManagerTest() {
    RegisterProfilePrefs(pref_service_.registry());
  }

  void SetUp() override {}

  std::unique_ptr<AdvancedProtectionStatusManagerPlatform> BuildManager(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager) {
    return std::unique_ptr<AdvancedProtectionStatusManagerDesktop>(
        new AdvancedProtectionStatusManagerDesktop(
            pref_service, identity_manager,
            base::TimeDelta() /*no min delay*/));
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

// We can only track the primary account.
TEST_F(AdvancedProtectionStatusManagerTest, TracksUnconsentedPrimaryAccount) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env(
      /*test_url_loader_factory=*/nullptr, &pref_service_);

  // Sign in, but don't set this as the primary account.
  AccountInfo account_info = identity_test_env.MakePrimaryAccountAvailable(
      "test@test.com", signin::ConsentLevel::kSignin);
  account_info.is_under_advanced_protection = true;
  identity_test_env.UpdateAccountInfoForAccount(account_info);
  auto manager =
      BuildManager(&pref_service_, identity_test_env.identity_manager());

  EXPECT_TRUE(manager->IsUnderAdvancedProtection());

  manager->Shutdown();
}

}  // namespace safe_browsing
