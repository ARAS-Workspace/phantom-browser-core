// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/new_tab_page_util.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time_override.h"
#include "base/values.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using ::base::Time;
using ::base::TimeDelta;

const char kSampleUserEmail[] = "user@gmail.com";

base::ListValue CreatePolicyList(const std::string& name,
                                 const std::string& url) {
  base::DictValue shortcut_item;
  shortcut_item.Set("name", name);
  shortcut_item.Set("url", url);
  base::ListValue policy_list;
  policy_list.Append(std::move(shortcut_item));
  return policy_list;
}

}  // namespace

class NewTabPageUtilBrowserTest : public SigninBrowserTestBase,
                                  public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    SigninBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();
    if (GetParam()) {
      SetAccountsCookiesAndTokens({kSampleUserEmail});
    }
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(optimization_guide::switches::kDebugLoggingEnabled);
  }

  OptimizationGuideKeyedService* GetOptimizationGuideKeyedService() {
    return OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile());
  }

  void CheckInternalsLog(std::string_view message) {
    auto* logger =
        GetOptimizationGuideKeyedService()->GetOptimizationGuideLogger();
    EXPECT_THAT(logger->recent_log_messages_,
                testing::Contains(testing::Field(
                    &OptimizationGuideLogger::LogMessage::message,
                    testing::HasSubstr(message))));
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

class NewTabPageUtilTileTypesEnterpriseShortcutsBrowserTest
    : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilTileTypesEnterpriseShortcutsBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_P(NewTabPageUtilTileTypesEnterpriseShortcutsBrowserTest,
                       GetEnabledTileTypes) {
  // By default, personal shortcuts are visible (Custom Links).
  EXPECT_EQ(GetEnabledTileTypes(browser()->GetProfile()),
            std::set<ntp_tiles::TileType>({ntp_tiles::TileType::kCustomLinks}));

  // Set enterprise shortcuts policy.
  browser()->GetProfile()->GetPrefs()->SetList(
      ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
      CreatePolicyList("work name", "https://work.com/"));

  // If enterprise shortcuts are also visible, both should be enabled.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      ntp_prefs::kNtpEnterpriseShortcutsVisible, true);
  EXPECT_EQ(GetEnabledTileTypes(browser()->GetProfile()),
            std::set<ntp_tiles::TileType>(
                {ntp_tiles::TileType::kCustomLinks,
                 ntp_tiles::TileType::kEnterpriseShortcuts}));

  // If personal shortcuts are explicitly hidden by the user,
  // only enterprise should remain.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      ntp_prefs::kNtpPersonalShortcutsVisible, false);
  EXPECT_EQ(GetEnabledTileTypes(browser()->GetProfile()),
            std::set<ntp_tiles::TileType>(
                {ntp_tiles::TileType::kEnterpriseShortcuts}));

  // Remove enterprise shortcuts policy, personal shortcuts should be visible.
  browser()->GetProfile()->GetPrefs()->SetList(
      ntp_tiles::prefs::kEnterpriseShortcutsPolicyList, base::ListValue());
  EXPECT_EQ(GetEnabledTileTypes(browser()->GetProfile()),
            std::set<ntp_tiles::TileType>({ntp_tiles::TileType::kCustomLinks}));
}

class NewTabPageUtilStalenessUpdateBrowserTest
    : public NewTabPageUtilBrowserTest {
 public:
  void SetUpOnMainThread() override {
    NewTabPageUtilBrowserTest::SetUpOnMainThread();
    current_time_ = Time::Now();
    time_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        &NewTabPageUtilStalenessUpdateBrowserTest::Now, nullptr, nullptr);
  }

  void TearDownOnMainThread() override {
    time_override_.reset();
    NewTabPageUtilBrowserTest::TearDownOnMainThread();
  }

  void InitMockShortcutsPrefs() {
    GetProfile()->GetPrefs()->SetTime(
        ntp_prefs::kNtpLastShortcutsStalenessUpdate, Time::Now());
    GetProfile()->GetPrefs()->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount,
                                         0);
    GetProfile()->GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
    GetProfile()->GetPrefs()->SetBoolean(
        ntp_prefs::kNtpShortcutsAutoRemovalDisabled, false);
  }

  // Advances the mock clock by the given time delta.
  void FastForwardBy(TimeDelta time_delta) { current_time_ += time_delta; }

  static Time Now() { return current_time_; }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  static Time current_time_;
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;
};

Time NewTabPageUtilStalenessUpdateBrowserTest::current_time_;

IN_PROC_BROWSER_TEST_P(NewTabPageUtilStalenessUpdateBrowserTest,
                       ShouldUpdateShortcutsStalenessWithNullTimestamp) {
  InitMockShortcutsPrefs();
  const bool is_null_timestamp = GetParam();
  if (is_null_timestamp) {
    GetProfile()->GetPrefs()->SetTime(
        ntp_prefs::kNtpLastShortcutsStalenessUpdate, Time());
  }

  const TimeDelta staleness_threshold =
      ntp_features::kShortcutsMinStalenessUpdateTimeInterval.Get();
  const TimeDelta time_delta = staleness_threshold + base::Seconds(1);

  FastForwardBy(time_delta);
  UpdateShortcutsStaleness(GetProfile());

  const Time updated_time = GetProfile()->GetPrefs()->GetTime(
      ntp_prefs::kNtpLastShortcutsStalenessUpdate);
  const int updated_count = GetProfile()->GetPrefs()->GetInteger(
      ntp_prefs::kNtpShortcutsStalenessCount);

  EXPECT_EQ(updated_time, Now());
  EXPECT_EQ(updated_count, is_null_timestamp ? 0 : 1);
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilStalenessUpdateBrowserTest,
                       ShouldUpdateShortcutsStalenessWithThreshold) {
  InitMockShortcutsPrefs();
  const bool is_above_update_threshold = GetParam();

  const TimeDelta staleness_threshold =
      ntp_features::kShortcutsMinStalenessUpdateTimeInterval.Get();
  const TimeDelta additional_time =
      is_above_update_threshold ? base::Seconds(1) : base::Seconds(-1);
  const TimeDelta time_delta = staleness_threshold + additional_time;
  const Time initial_time = GetProfile()->GetPrefs()->GetTime(
      ntp_prefs::kNtpLastShortcutsStalenessUpdate);

  FastForwardBy(time_delta);
  UpdateShortcutsStaleness(GetProfile());

  const Time updated_time = GetProfile()->GetPrefs()->GetTime(
      ntp_prefs::kNtpLastShortcutsStalenessUpdate);
  const int updated_count = GetProfile()->GetPrefs()->GetInteger(
      ntp_prefs::kNtpShortcutsStalenessCount);

  EXPECT_EQ(updated_time, is_above_update_threshold ? Now() : initial_time);
  EXPECT_EQ(updated_count, is_above_update_threshold ? 1 : 0);

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.MostVisited.AutoRemovalSkipped",
      NtpShortcutsAutoRemovalReason::kStaleDaysCount,
      is_above_update_threshold ? 1 : 0);
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilStalenessUpdateBrowserTest,
                       ShouldUpdateShortcutsStalenessWithAutoRemovalDisabled) {
  InitMockShortcutsPrefs();
  const bool is_auto_removal_disabled = GetParam();
  GetProfile()->GetPrefs()->SetBoolean(
      ntp_prefs::kNtpShortcutsAutoRemovalDisabled, is_auto_removal_disabled);

  const TimeDelta staleness_threshold =
      ntp_features::kShortcutsMinStalenessUpdateTimeInterval.Get();
  const TimeDelta time_delta = staleness_threshold + base::Seconds(1);
  const Time initial_time = GetProfile()->GetPrefs()->GetTime(
      ntp_prefs::kNtpLastShortcutsStalenessUpdate);

  FastForwardBy(time_delta);
  UpdateShortcutsStaleness(GetProfile());

  const Time updated_time = GetProfile()->GetPrefs()->GetTime(
      ntp_prefs::kNtpLastShortcutsStalenessUpdate);
  const int updated_count = GetProfile()->GetPrefs()->GetInteger(
      ntp_prefs::kNtpShortcutsStalenessCount);

  EXPECT_EQ(updated_time, is_auto_removal_disabled ? initial_time : Now());
  EXPECT_EQ(updated_count, is_auto_removal_disabled ? 0 : 1);

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.MostVisited.AutoRemovalSkipped",
      NtpShortcutsAutoRemovalReason::kDisabled,
      is_auto_removal_disabled ? 1 : 0);
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilStalenessUpdateBrowserTest,
                       ShouldUpdateShortcutsStalenessWithShortcutsHidden) {
  InitMockShortcutsPrefs();
  const bool are_shortcuts_hidden = GetParam();
  GetProfile()->GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible,
                                       !are_shortcuts_hidden);

  const TimeDelta staleness_threshold =
      ntp_features::kShortcutsMinStalenessUpdateTimeInterval.Get();
  const TimeDelta time_delta = staleness_threshold + base::Seconds(1);
  const Time initial_time = GetProfile()->GetPrefs()->GetTime(
      ntp_prefs::kNtpLastShortcutsStalenessUpdate);

  FastForwardBy(time_delta);
  UpdateShortcutsStaleness(GetProfile());

  const Time updated_time = GetProfile()->GetPrefs()->GetTime(
      ntp_prefs::kNtpLastShortcutsStalenessUpdate);
  const int updated_count = GetProfile()->GetPrefs()->GetInteger(
      ntp_prefs::kNtpShortcutsStalenessCount);

  EXPECT_EQ(updated_time, are_shortcuts_hidden ? initial_time : Now());
  EXPECT_EQ(updated_count, are_shortcuts_hidden ? 0 : 1);

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.MostVisited.AutoRemovalSkipped",
      NtpShortcutsAutoRemovalReason::kNotVisible, are_shortcuts_hidden ? 1 : 0);
}

// Parameterized to test for shortcuts with managed preference.
IN_PROC_BROWSER_TEST_P(NewTabPageUtilStalenessUpdateBrowserTest,
                       ShouldUpdateShortcutsStalenessWithManagedPreference) {
  // Arrange.
  InitMockShortcutsPrefs();
  const bool is_managed_preference = GetParam();
  if (is_managed_preference) {
    GetProfile()->GetPrefs()->SetList(
        ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
        CreatePolicyList("work name", "https://work.com/"));
    GetProfile()->GetPrefs()->SetBoolean(
        ntp_prefs::kNtpEnterpriseShortcutsVisible, true);
  }

  const TimeDelta staleness_threshold =
      ntp_features::kShortcutsMinStalenessUpdateTimeInterval.Get();
  const TimeDelta time_delta = staleness_threshold + base::Seconds(1);

  // Act.
  FastForwardBy(time_delta);
  UpdateShortcutsStaleness(GetProfile());

  // Assert.
  histogram_tester_.ExpectBucketCount(
      "NewTabPage.MostVisited.AutoRemovalSkipped",
      NtpShortcutsAutoRemovalReason::kManagedPreference,
      is_managed_preference ? 1 : 0);
}

// Parameterized to test for logging the shortcuts staleness count metric.
IN_PROC_BROWSER_TEST_P(NewTabPageUtilStalenessUpdateBrowserTest,
                       ShouldLogShortcutsStalenessCountMetric) {
  // Arrange.
  InitMockShortcutsPrefs();
  const bool is_above_staleness_count = GetParam();
  const int shortcuts_staleness_count = is_above_staleness_count ? 15 : 0;
  if (is_above_staleness_count) {
    GetProfile()->GetPrefs()->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount,
                                         shortcuts_staleness_count);
  }

  const TimeDelta staleness_threshold =
      ntp_features::kShortcutsMinStalenessUpdateTimeInterval.Get();
  const TimeDelta time_delta = staleness_threshold + base::Seconds(1);

  // Act.
  FastForwardBy(time_delta);
  UpdateShortcutsStaleness(GetProfile());

  // Assert.
  EXPECT_EQ(GetProfile()->GetPrefs()->GetInteger(
                ntp_prefs::kNtpShortcutsStalenessCount),
            shortcuts_staleness_count + 1);

  histogram_tester_.ExpectUniqueSample(
      "NewTabPage.MostVisited.AutoRemovalStaleDays", shortcuts_staleness_count,
      1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         NewTabPageUtilTileTypesEnterpriseShortcutsBrowserTest,
                         testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         NewTabPageUtilStalenessUpdateBrowserTest,
                         testing::Bool());
