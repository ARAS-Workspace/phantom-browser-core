// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blocklist_navigation_throttle.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "components/policy/core/browser/url_list/url_blocklist_policy_handler.h"
#include "components/policy/core/browser/url_list/url_list_policy_pref_names.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

struct PolicyBlocklistTestParams {
  bool is_incognito_mode;
};

class PolicyBlocklistNavigationThrottleTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<PolicyBlocklistTestParams> {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    user_prefs::UserPrefs::Set(browser_context(), &pref_service_);
    policy::URLBlocklistManager::RegisterProfilePrefs(pref_service_.registry());

    // TODO(crbug.com/442891187): Remove this once the prefs are registered in
    // the URLBlocklistManager::RegisterProfilePrefs.
    pref_service_.registry()->RegisterListPref(
        policy::policy_prefs::kIncognitoModeUrlBlocklist);
    pref_service_.registry()->RegisterListPref(
        policy::policy_prefs::kIncognitoModeUrlAllowlist);

    auto url_blocklist_manager = std::make_unique<policy::URLBlocklistManager>(
        &pref_service_, policy::policy_prefs::kUrlBlocklist,
        policy::policy_prefs::kUrlAllowlist);
    std::unique_ptr<policy::URLBlocklistManager>
        incognito_url_blocklist_manager;
    if (IsIncognitoMode()) {
      incognito_url_blocklist_manager =
          std::make_unique<policy::URLBlocklistManager>(
              &pref_service_, policy::policy_prefs::kIncognitoModeUrlBlocklist,
              policy::policy_prefs::kIncognitoModeUrlAllowlist);
    }

    policy_blocklist_service_ = std::make_unique<PolicyBlocklistService>(
        std::move(url_blocklist_manager),
        std::move(incognito_url_blocklist_manager), &pref_service_);
  }

  void TearDown() override {
    policy_blocklist_service_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<content::NavigationSimulator> StartNavigation(
      const GURL& first_url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(first_url,
                                                              main_rfh());
    auto throttle_inserter =
        std::make_unique<content::TestNavigationThrottleInserter>(
            web_contents(),
            base::BindRepeating(
                &PolicyBlocklistNavigationThrottleTest::CreateAndAddThrottle,
                base::Unretained(this)));
    navigation_simulator->SetAutoAdvance(false);
    navigation_simulator->Start();
    return navigation_simulator;
  }

  void CreateAndAddThrottle(content::NavigationThrottleRegistry& registry) {
    registry.AddThrottle(std::make_unique<PolicyBlocklistNavigationThrottle>(
        registry, policy_blocklist_service_.get()));
  }

  void SetBlocklistUrlPattern(const std::string& pattern) {
    base::ListValue value;
    value.Append(pattern);
    pref_service_.SetManagedPref(policy::policy_prefs::kUrlBlocklist,
                                 std::move(value));
    task_environment()->RunUntilIdle();
  }

  void SetAllowlistUrlPattern(const std::string& pattern) {
    base::ListValue value;
    value.Append(pattern);
    pref_service_.SetManagedPref(policy::policy_prefs::kUrlAllowlist,
                                 std::move(value));
    task_environment()->RunUntilIdle();
  }

  void SetIncognitoBlocklistUrlPattern(const std::string& pattern) {
    base::ListValue value;
    value.Append(pattern);
    pref_service_.SetManagedPref(
        policy::policy_prefs::kIncognitoModeUrlBlocklist, std::move(value));
    task_environment()->RunUntilIdle();
  }

  void SetIncognitoAllowlistUrlPattern(const std::string& pattern) {
    base::ListValue value;
    value.Append(pattern);
    pref_service_.SetManagedPref(
        policy::policy_prefs::kIncognitoModeUrlAllowlist, std::move(value));
    task_environment()->RunUntilIdle();
  }

  void TestNavigationThrottleCheckResult(
      const GURL& url,
      content::NavigationThrottle::ThrottleAction expected_action,
      std::optional<net::Error> expected_error = std::nullopt) {
    auto navigation_simulator = StartNavigation(url);
    ASSERT_FALSE(navigation_simulator->IsDeferred());

    EXPECT_EQ(expected_action,
              navigation_simulator->GetLastThrottleCheckResult().action());
    if (expected_error.has_value()) {
      EXPECT_EQ(
          expected_error,
          navigation_simulator->GetLastThrottleCheckResult().net_error_code());
    }

    // Call WebContents::Stop() to reset the main rfh's navigation state. It
    // results in destructing the navigation throttles to flush metrics.
    RenderViewHostTestHarness::web_contents()->Stop();
  }

  bool IsIncognitoMode() { return GetParam().is_incognito_mode; }

  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<PolicyBlocklistService> policy_blocklist_service_;
};

TEST_P(PolicyBlocklistNavigationThrottleTest, Blocklist) {
  base::HistogramTester histogram_tester;

  SetBlocklistUrlPattern("example.com");

  // Block a blocklisted site.
  TestNavigationThrottleCheckResult(GURL("http://www.example.com/"),
                                    content::NavigationThrottle::BLOCK_REQUEST,
                                    net::ERR_BLOCKED_BY_ADMINISTRATOR);
}

TEST_P(PolicyBlocklistNavigationThrottleTest, Allowlist) {
  base::HistogramTester histogram_tester;

  SetAllowlistUrlPattern("www.example.com");
  SetBlocklistUrlPattern("example.com");

  // Allow a allowlisted exception to a blocklisted domain.
  TestNavigationThrottleCheckResult(GURL("http://www.example.com/"),
                                    content::NavigationThrottle::PROCEED);
}

TEST_P(PolicyBlocklistNavigationThrottleTest, IncognitoBlocklist) {
  base::HistogramTester histogram_tester;

  SetIncognitoBlocklistUrlPattern("example.com");

  // Block a blocklisted site in incognito mode and allow in regular mode.
  TestNavigationThrottleCheckResult(
      GURL("http://www.example.com/"),
      IsIncognitoMode() ? content::NavigationThrottle::BLOCK_REQUEST
                        : content::NavigationThrottle::PROCEED,
      IsIncognitoMode()
          ? std::make_optional(net::ERR_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR)
          : std::nullopt);
}

TEST_P(PolicyBlocklistNavigationThrottleTest,
       IncognitoAllowlistAgainstIncognitoBlocklist) {
  base::HistogramTester histogram_tester;

  SetIncognitoAllowlistUrlPattern("www.example.com");
  SetIncognitoBlocklistUrlPattern("example.com");
  TestNavigationThrottleCheckResult(GURL("http://www.example.com/"),
                                    content::NavigationThrottle::PROCEED);
}

TEST_P(PolicyBlocklistNavigationThrottleTest,
       IncognitoAllowlistAgainstURLBlocklistFeatureEnabled) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      policy::features::kURLBlocklistOverridesIncognitoAllowlist);

  base::HistogramTester histogram_tester;

  SetIncognitoAllowlistUrlPattern("www.example.com");
  SetBlocklistUrlPattern("example.com");

  // General blocklists cannot be bypassed by an incognito allowlist by default.
  TestNavigationThrottleCheckResult(
      GURL("http://www.example.com/"),
      content::NavigationThrottle::BLOCK_REQUEST,
      std::make_optional(net::ERR_BLOCKED_BY_ADMINISTRATOR));
}

TEST_P(PolicyBlocklistNavigationThrottleTest,
       IncognitoAllowlistAgainstURLBlocklistFeatureDisabled) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(
      policy::features::kURLBlocklistOverridesIncognitoAllowlist);

  base::HistogramTester histogram_tester;

  SetIncognitoAllowlistUrlPattern("www.example.com");
  SetBlocklistUrlPattern("example.com");

  // Allow a allowlisted exception to a regular blocklist in incognito mode and
  // block in regular mode.
  TestNavigationThrottleCheckResult(
      GURL("http://www.example.com/"),
      IsIncognitoMode() ? content::NavigationThrottle::PROCEED
                        : content::NavigationThrottle::BLOCK_REQUEST,
      IsIncognitoMode()
          ? std::nullopt
          : std::make_optional(net::ERR_BLOCKED_BY_ADMINISTRATOR));
}

TEST_P(PolicyBlocklistNavigationThrottleTest,
       URLAllowlistAgainstIncognitoBlocklist) {
  base::HistogramTester histogram_tester;

  SetAllowlistUrlPattern("www.example.com");
  SetIncognitoBlocklistUrlPattern("example.com");

  // A regular URL allowlist against an Incognito blocklist should block in
  // incognito mode.
  TestNavigationThrottleCheckResult(
      GURL("http://www.example.com/"),
      IsIncognitoMode() ? content::NavigationThrottle::BLOCK_REQUEST
                        : content::NavigationThrottle::PROCEED,
      IsIncognitoMode()
          ? std::make_optional(net::ERR_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR)
          : std::nullopt);
}

// Run all PolicyBlocklistNavigationThrottle tests for both incognito and
// non-Incognito modes.
INSTANTIATE_TEST_SUITE_P(
    All,
    PolicyBlocklistNavigationThrottleTest,
    testing::Values(PolicyBlocklistTestParams{.is_incognito_mode = false},
                    PolicyBlocklistTestParams{.is_incognito_mode = true}));
