// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/enterprise_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
extensions::SafeBrowsingPrivateEventRouter* GetSafeBrowsingEventRouter(
    content::WebContents* web_contents) {
  // |web_contents| can be null in tests.
  if (!web_contents) {
    return nullptr;
  }

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // In guest profile, IsOffTheRecord also returns true. So we need an
  // additional check on IsGuestSession to ensure the event is sent in guest
  // mode.
  if (profile->IsOffTheRecord() && !profile->IsGuestSession()) {
    return nullptr;
  }

  return extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
      browser_context);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)

}  // namespace

void MaybeTriggerSecurityInterstitialShownEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& reason,
    int net_error_code) {
#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  extensions::SafeBrowsingPrivateEventRouter* safe_browsing_event_router =
      GetSafeBrowsingEventRouter(web_contents);
  if (!safe_browsing_event_router) {
    return;
  }

  safe_browsing_event_router->OnSecurityInterstitialShown(page_url, reason,
                                                          net_error_code);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
}

void MaybeTriggerSecurityInterstitialProceededEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& reason,
    int net_error_code) {
#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  extensions::SafeBrowsingPrivateEventRouter* safe_browsing_event_router =
      GetSafeBrowsingEventRouter(web_contents);
  if (!safe_browsing_event_router) {
    return;
  }
  safe_browsing_event_router->OnSecurityInterstitialProceeded(page_url, reason,
                                                              net_error_code);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
void MaybeTriggerUrlFilteringInterstitialEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& threat_type,
    safe_browsing::RTLookupResponse rt_lookup_response) {

}
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)
