// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/tailored_security_url_observer.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/tailored_security/notification_handler_desktop.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_observer_util.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_widget_host_view.h"

#include "chrome/browser/ui/views/safe_browsing/tailored_security_unconsented_modal.h"

namespace safe_browsing {

TailoredSecurityUrlObserver::~TailoredSecurityUrlObserver() {
  if (service_) {
    service_->RemoveObserver(this);
    if (has_query_request_) {
      service_->RemoveQueryRequest();
      has_query_request_ = false;
    }
  }
}

// content::WebContentsObserver:
void TailoredSecurityUrlObserver::PrimaryPageChanged(content::Page& page) {
  UpdateFocusAndURL(true, page.GetMainDocument().GetLastCommittedURL());
}

void TailoredSecurityUrlObserver::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  UpdateFocusAndURL(true, last_url_);
}

void TailoredSecurityUrlObserver::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  UpdateFocusAndURL(false, last_url_);
}

void TailoredSecurityUrlObserver::OnTailoredSecurityBitChanged(
    bool enabled,
    base::Time previous_update) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!enabled || !CanShowUnconsentedTailoredSecurityDialog(
                      sync_service, profile->GetPrefs())) {
    return;
  }

  profile->GetPrefs()->SetBoolean(
      prefs::kAccountTailoredSecurityShownNotification, true);

  if (base::Time::Now() - previous_update <= kThresholdForInFlowNotification) {
      TailoredSecurityUnconsentedModal::ShowForWebContents(web_contents());
  } else {
    DisplayTailoredSecurityUnconsentedPromotionNotification(profile);
  }
}

void TailoredSecurityUrlObserver::OnTailoredSecurityServiceDestroyed() {
  service_->RemoveObserver(this);
  service_ = nullptr;
}

TailoredSecurityUrlObserver::TailoredSecurityUrlObserver(
    content::WebContents* web_contents,
    TailoredSecurityService* service)
    : WebContentsObserver(web_contents),
      WebContentsUserData(*web_contents),
      service_(service) {
  bool focused = false;

  if (service_) {
    service_->AddObserver(this);
  }

  if (web_contents && web_contents->GetPrimaryMainFrame() &&
      web_contents->GetPrimaryMainFrame()->GetView()) {
    focused = web_contents->GetPrimaryMainFrame()->GetView()->HasFocus();
  }
  UpdateFocusAndURL(focused, web_contents->GetLastCommittedURL());
}

void TailoredSecurityUrlObserver::UpdateFocusAndURL(bool focused,
                                                    const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!CanShowUnconsentedTailoredSecurityDialog(sync_service,
                                                profile->GetPrefs())) {
    return;
  }

  if (service_) {
    bool should_query = focused && CanQueryTailoredSecurityForUrl(url);
    bool old_should_query =
        focused_ && CanQueryTailoredSecurityForUrl(last_url_);
    if (should_query && !old_should_query) {
      has_query_request_ = service_->AddQueryRequest();
    }
    if (!should_query && old_should_query && has_query_request_) {
      service_->RemoveQueryRequest();
      has_query_request_ = false;
    }
  }

  focused_ = focused;
  last_url_ = url;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TailoredSecurityUrlObserver);

}  // namespace safe_browsing
