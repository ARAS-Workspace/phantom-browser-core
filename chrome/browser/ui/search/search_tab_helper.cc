// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/webui/theme_favicon_source_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/search/search.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NewTabPageConcretePage {
  kOther = 0,
  k1PWebUiNtp = 1,
  k3PWebUiNtp = 2,
  k3PRemoteNtp = 3,
  kExtensionNtp = 4,
  kOffTheRecordNtp = 5,
  kMaxValue = kOffTheRecordNtp,
};

void RecordConcreteNtp(content::NavigationHandle* navigation_handle) {
  NewTabPageConcretePage concrete_page = NewTabPageConcretePage::kOther;
  if (navigation_handle->GetURL().DeprecatedGetOriginAsURL() ==
      GURL(chrome::kChromeUINewTabPageThirdPartyURL)
          .DeprecatedGetOriginAsURL()) {
    concrete_page = NewTabPageConcretePage::k3PWebUiNtp;
  } else if (navigation_handle->GetURL().SchemeIs(
                 extensions::kExtensionScheme)) {
    concrete_page = NewTabPageConcretePage::kExtensionNtp;
  } else if (Profile::FromBrowserContext(
                 navigation_handle->GetWebContents()->GetBrowserContext())
                 ->IsOffTheRecord() &&
             navigation_handle->GetURL().DeprecatedGetOriginAsURL() ==
                 chrome::ChromeUINewTabURLAsGURL().DeprecatedGetOriginAsURL()) {
    concrete_page = NewTabPageConcretePage::kOffTheRecordNtp;
  }
  base::UmaHistogramEnumeration("NewTabPage.ConcretePage", concrete_page);
}

}  // namespace

SearchTabHelper::SearchTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<SearchTabHelper>(*web_contents) {
  DCHECK(search::IsInstantExtendedAPIEnabled());

  // ThemeFaviconSourceService adds the theme and favicon data sources of the
  // profile.
  ThemeFaviconSourceServiceFactory::GetForProfile(profile());

  OmniboxTabHelper::CreateForWebContents(web_contents, profile());
}

SearchTabHelper::~SearchTabHelper() = default;

void SearchTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (navigation_handle->IsSameDocument()) {
    return;
  }

  if (web_contents()->GetVisibleURL().DeprecatedGetOriginAsURL() ==
      chrome::ChromeUINewTabURLAsGURL().DeprecatedGetOriginAsURL()) {
    RecordConcreteNtp(navigation_handle);
  }

  if (search::IsNTPOrRelatedURL(navigation_handle->GetURL(), profile())) {
    // Set the title on any pending entry corresponding to the NTP. This
    // prevents any flickering of the tab title.
    content::NavigationEntry* entry =
        web_contents()->GetController().GetPendingEntry();
    if (entry) {
      web_contents()->UpdateTitleForEntry(
          entry, l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
    }
  }
}

Profile* SearchTabHelper::profile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchTabHelper);
