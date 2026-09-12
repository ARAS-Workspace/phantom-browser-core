// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_NAVIGATION_THROTTLE_H_
#define COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_NAVIGATION_THROTTLE_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "content/public/browser/navigation_throttle.h"

class GURL;
class PolicyBlocklistService;

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

// PolicyBlocklistNavigationThrottle provides a simple way to block a navigation
// based on the URLBlocklistManager. If the URL is on the blocklist or
// allowlist, the throttle will immediately block or allow the navigation.
class PolicyBlocklistNavigationThrottle : public content::NavigationThrottle {
 public:
  PolicyBlocklistNavigationThrottle(
      content::NavigationThrottleRegistry& registry,
      PolicyBlocklistService* blocklist_service);
  PolicyBlocklistNavigationThrottle(const PolicyBlocklistNavigationThrottle&) =
      delete;
  PolicyBlocklistNavigationThrottle& operator=(
      const PolicyBlocklistNavigationThrottle&) = delete;
  ~PolicyBlocklistNavigationThrottle() override;

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PolicyBlocklistNavigationThrottleTest, Blocklist);
  FRIEND_TEST_ALL_PREFIXES(PolicyBlocklistNavigationThrottleTest, Allowlist);

  // Returns TRUE if this navigation is to view-source.
  bool IsViewSourceNavigation();

  // Returns the PolicyBlocklistState for a view-source navigation.
  // Should only be called if the navigation is a view-source.
  PolicyBlocklistService::PolicyBlocklistState
  GetViewSourceNavigationBlocklistState();

  ThrottleCheckResult WillStartOrRedirectRequest();

  const raw_ptr<PolicyBlocklistService, DanglingUntriaged> blocklist_service_;
};

#endif  // COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_NAVIGATION_THROTTLE_H_
