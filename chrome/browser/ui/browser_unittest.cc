// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserUnitTest : public testing::Test {
 public:
  BrowserUnitTest() = default;
  ~BrowserUnitTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(BrowserUnitTest, CreateBrowserFailsIfProfileDisallowsBrowserWindows) {
  TestingProfile::Builder profile_builder;
  profile_builder.DisallowBrowserWindows();
  std::unique_ptr<TestingProfile> test_profile = profile_builder.Build();
  TestingProfile::Builder otr_profile_builder;
  otr_profile_builder.DisallowBrowserWindows();
  otr_profile_builder.BuildIncognito(test_profile.get());

  // Verify creating browser fails in both original and OTR version of the
  // profile.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable,
            GetBrowserWindowCreationStatusForProfile(*test_profile.get()));
  EXPECT_EQ(
      BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable,
      GetBrowserWindowCreationStatusForProfile(
          *test_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

// Tests GetBrowserWindowCreationStatusForProfile() when Incognito mode is
// disabled.
TEST_F(BrowserUnitTest, CreateBrowserWithIncognitoModeDisabled) {
  IncognitoModePrefs::SetAvailability(
      profile_.GetPrefs(), policy::IncognitoModeAvailability::kDisabled);

  // Creating a browser window in OTR profile should fail if incognito is
  // disabled.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable,
            GetBrowserWindowCreationStatusForProfile(
                *profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  // Verify creating a browser in the original profile is allowed.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(profile_));
}

// Tests GetBrowserWindowCreationStatusForProfile() when Incognito mode is
// forced.
TEST_F(BrowserUnitTest, CreateBrowserWithIncognitoModeForced) {
  IncognitoModePrefs::SetAvailability(
      profile_.GetPrefs(), policy::IncognitoModeAvailability::kForced);

  // Creating a browser window in the original profile should fail if incognito
  // is forced.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable,
            GetBrowserWindowCreationStatusForProfile(profile_));

  // Creating a browser in OTR test profile should succeed.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(
                *profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

// Tests GetBrowserWindowCreationStatusForProfile() with no restrictions on
// incognito mode.
TEST_F(BrowserUnitTest, CreateBrowserWithIncognitoModeEnabled) {
  ASSERT_EQ(policy::IncognitoModeAvailability::kEnabled,
            IncognitoModePrefs::GetAvailability(profile_.GetPrefs()));

  // Creating a browser in the original test profile should succeed.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(profile_));

  // Creating a browser in OTR test profile should succeed.
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(
                *profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}
