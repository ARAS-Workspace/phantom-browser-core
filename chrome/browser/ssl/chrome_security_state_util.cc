// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_state_util.h"

#include "chrome/browser/lookalikes/safety_tip_web_contents_observer.h"
#include "chrome/browser/net/qwac_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/content/content_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace chrome_security_state {

std::unique_ptr<security_state::VisibleSecurityState> GetVisibleSecurityState(
    content::WebContents* web_contents) {
  auto state = security_state::GetVisibleSecurityState(web_contents);

  // Get the 2-QWAC state.
  QwacWebContentsObserver::QwacStatus* qwac_status =
      QwacWebContentsObserver::QwacStatus::GetForPage(
          web_contents->GetPrimaryPage());
  if (qwac_status && qwac_status->is_finished()) {
    state->two_qwac = qwac_status->verified_2qwac_cert();
  }

  SafetyTipWebContentsObserver* safety_tip_web_contents_observer =
      SafetyTipWebContentsObserver::FromWebContents(web_contents);
  state->safety_tip_info =
      safety_tip_web_contents_observer
          ? safety_tip_web_contents_observer
                ->GetSafetyTipInfoForVisibleNavigation()
          : security_state::SafetyTipInfo(
                {security_state::SafetyTipStatus::kUnknown, GURL()});

  // If both the mixed form warnings are not disabled by policy we don't degrade
  // the lock icon for sites with mixed forms present.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile &&
      profile->GetPrefs()->GetBoolean(prefs::kMixedFormsWarningsEnabled)) {
    state->should_treat_displayed_mixed_forms_as_secure = true;
  }

  // TODO(crbug.com/40248833): Track upgrade/fallback state per-navigation.
  // Currently HTTPS Upgrades state is tracked via a TabHelper attached to the
  // current WebContents (i.e., per tab), which can cause this state to "leak"
  // across multiple different navigations, potentially causing the wrong
  // security state to be computed.
  auto* https_only_mode_tab_helper =
      HttpsOnlyModeTabHelper::FromWebContents(web_contents);
  if (https_only_mode_tab_helper &&
      (https_only_mode_tab_helper->is_navigation_upgraded() ||
       https_only_mode_tab_helper->is_navigation_fallback())) {
    state->is_https_only_mode_upgraded = true;
  }

  return state;
}

security_state::SecurityLevel GetSecurityLevel(
    content::WebContents* web_contents) {
  return security_state::GetSecurityLevel(
      *GetVisibleSecurityState(web_contents));
}

}  // namespace chrome_security_state
