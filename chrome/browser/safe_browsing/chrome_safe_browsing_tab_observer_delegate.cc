// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_tab_observer_delegate.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/buildflags.h"

namespace safe_browsing {

ChromeSafeBrowsingTabObserverDelegate::ChromeSafeBrowsingTabObserverDelegate() =
    default;
ChromeSafeBrowsingTabObserverDelegate::
    ~ChromeSafeBrowsingTabObserverDelegate() = default;

PrefService* ChromeSafeBrowsingTabObserverDelegate::GetPrefs(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)->GetPrefs();
}

ClientSideDetectionService*
ChromeSafeBrowsingTabObserverDelegate::GetClientSideDetectionServiceIfExists(
    content::BrowserContext* browser_context) {
  return nullptr;
}

bool ChromeSafeBrowsingTabObserverDelegate::DoesSafeBrowsingServiceExist() {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  return g_browser_process->safe_browsing_service();
#else
  return false;
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)
}

std::unique_ptr<ClientSideDetectionHost>
ChromeSafeBrowsingTabObserverDelegate::CreateClientSideDetectionHost(
    content::WebContents* web_contents) {
  return nullptr;
}

}  // namespace safe_browsing
