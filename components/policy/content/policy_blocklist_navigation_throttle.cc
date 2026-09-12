// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blocklist_navigation_throttle.h"

#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

using URLBlocklistState = policy::URLBlocklist::URLBlocklistState;
using PolicyBlocklistState = PolicyBlocklistService::PolicyBlocklistState;

PolicyBlocklistNavigationThrottle::PolicyBlocklistNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    PolicyBlocklistService* blocklist_service)
    : content::NavigationThrottle(registry),
      blocklist_service_(blocklist_service) {}
PolicyBlocklistNavigationThrottle::~PolicyBlocklistNavigationThrottle() =
    default;

bool PolicyBlocklistNavigationThrottle::IsViewSourceNavigation() {
  content::NavigationEntry* nav_entry =
      navigation_handle()->GetNavigationEntry();
  return nav_entry && nav_entry->IsViewSourceMode();
}

PolicyBlocklistState
PolicyBlocklistNavigationThrottle::GetViewSourceNavigationBlocklistState() {
  CHECK(IsViewSourceNavigation());
  GURL view_source_url =
      GURL(std::string("view-source:") + navigation_handle()->GetURL().spec());

  return blocklist_service_->GetURLBlocklistStateWithPolicySource(
      view_source_url);
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillStartOrRedirectRequest() {
  // Ignore blob scheme because we may use it to deliver navigation responses
  // to the renderer process.
  const GURL& url = navigation_handle()->GetURL();
  if (url.SchemeIs(url::kBlobScheme)) {
    return PROCEED;
  }

  PolicyBlocklistState blocklist_state =
      blocklist_service_->GetURLBlocklistStateWithPolicySource(url);

  if (blocklist_state.url_blocklist_state ==
      URLBlocklistState::URL_IN_BLOCKLIST) {
    return ThrottleCheckResult(
        BLOCK_REQUEST,
        blocklist_state.policy_source == PolicyBlocklistState::INCOGNITO_POLICY
            ? net::ERR_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR
            : net::ERR_BLOCKED_BY_ADMINISTRATOR);
  }

  // If the navigation is to view-source, check if the view-source:url should
  // be blocked.
  if (IsViewSourceNavigation()) {
    PolicyBlocklistState view_source_blocklist_state =
        GetViewSourceNavigationBlocklistState();
    if (view_source_blocklist_state.url_blocklist_state ==
        URLBlocklistState::URL_IN_BLOCKLIST) {
      return ThrottleCheckResult(
          BLOCK_REQUEST, view_source_blocklist_state.policy_source ==
                                 PolicyBlocklistState::INCOGNITO_POLICY
                             ? net::ERR_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR
                             : net::ERR_BLOCKED_BY_ADMINISTRATOR);
    }
  }

  if (blocklist_state.url_blocklist_state ==
      URLBlocklistState::URL_IN_ALLOWLIST) {
    return PROCEED;
  }

  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest();
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest();
}

const char* PolicyBlocklistNavigationThrottle::GetNameForLogging() {
  return "PolicyBlocklistNavigationThrottle";
}
