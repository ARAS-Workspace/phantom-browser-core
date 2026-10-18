// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"

#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "extensions/common/constants.h"

namespace ntp_footer {

bool IsExtensionNtp(const GURL& url, Profile* profile) {
  if (!url.SchemeIs(extensions::kExtensionScheme)) {
    return false;
  }

  const extensions::Extension* extension_managing_ntp =
      extensions::GetExtensionOverridingNewTabPage(profile);

  if (!extension_managing_ntp) {
    return false;
  }

  return extension_managing_ntp->id() == url.GetHost();
}

bool IsNtp(const GURL& url,
           content::WebContents* web_contents,
           Profile* profile) {
  return NewTabUI::IsNewTab(url) ||
         NewTabPageThirdPartyUI::IsNewTabPageOrigin(url) ||
         ntp_footer::IsExtensionNtp(url, profile);
}
}  // namespace ntp_footer
