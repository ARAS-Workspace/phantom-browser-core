// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include <stddef.h>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"

#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"

namespace search {

namespace {

bool MatchesOrigin(const GURL& my_url, const GURL& other_url) {
  return my_url.scheme() == other_url.scheme() &&
         my_url.host() == other_url.host() && my_url.port() == other_url.port();
}

}  // namespace

// Returns true if |my_url| matches |other_url| in terms of origin (i.e. host,
// port, and scheme) and path.
// Defined outside of the anonymous namespace so that it's accessible to unit
// tests.
bool MatchesOriginAndPath(const GURL& my_url, const GURL& other_url) {
  return MatchesOrigin(my_url, other_url) && my_url.path() == other_url.path();
}

namespace {

// Status of the New Tab URL for the default Search provider. NOTE: Used in a
// UMA histogram so values should only be added at the end and not reordered.
enum NewTabURLState {
  // Valid URL that should be used.
  NEW_TAB_URL_VALID = 0,

  // Corrupt state (e.g. no profile or template url).
  NEW_TAB_URL_BAD = 1,

  // URL should not be used because in incognito window.
  NEW_TAB_URL_INCOGNITO = 2,

  // No New Tab URL set for provider.
  NEW_TAB_URL_NOT_SET = 3,

  // URL is not secure.
  NEW_TAB_URL_INSECURE = 4,

  // URL should not be used because Suggest is disabled.
  // Not used anymore, see crbug.com/40350179.
  // NEW_TAB_URL_SUGGEST_OFF = 5,

  // URL should not be used because it is blocked for a supervised user.
  NEW_TAB_URL_BLOCKED = 6,

  NEW_TAB_URL_MAX
};

// Returns true if |url| matches the NTP URL.
bool IsNTPOrRelatedURLHelper(const GURL& url, Profile* profile) {
  if (!url.is_valid()) {
    return false;
  }

  const GURL new_tab_url(GetNewTabPageURL(profile));
  return new_tab_url.is_valid() && MatchesOriginAndPath(url, new_tab_url);
}

// Used to look up the URL to use for the New Tab page. Also tracks how we
// arrived at that URL so it can be logged with UMA.
struct NewTabURLDetails {
  NewTabURLDetails(const GURL& url, NewTabURLState state)
      : url(url), state(state) {}

  static NewTabURLDetails ForProfile(Profile* profile) {
    // Incognito and Guest profiles have their own New Tab.
    // This function may also be called by other off-the-record profiles that
    // can exceptionally open a browser window.
    // See OTRProfileID::AllowsBrowserWindows() for more context.
    if (profile->IsOffTheRecord()) {
      return NewTabURLDetails(GURL(), NEW_TAB_URL_INCOGNITO);
    }

    const GURL local_url(chrome::kChromeUINewTabPageThirdPartyURL);
    return NewTabURLDetails(local_url, NEW_TAB_URL_VALID);
  }

  const GURL url;
  const NewTabURLState state;
};

}  // namespace

bool DefaultSearchProviderIsGoogle(Profile* profile) {
  return DefaultSearchProviderIsGoogle(
      TemplateURLServiceFactory::GetForProfile(profile));
}

bool IsNTPOrRelatedURL(const GURL& url, Profile* profile) {
  if (!url.is_valid()) {
    return false;
  }

  if (!IsInstantExtendedAPIEnabled()) {
    return url == chrome::ChromeUINewTabURLAsGURL();
  }

  return profile && IsNTPOrRelatedURLHelper(url, profile);
}

bool IsNTPURL(const GURL& url) {
  return NewTabPageThirdPartyUI::IsNewTabPageOrigin(url);
}

bool IsWebUiNtpEnabledForDesktopAndroid() {
  return false;
}

bool IsSplitViewNewTabPage(const GURL& url) {
  return url.spec() == chrome::kChromeUISplitViewNewTabPageURL;
}

GURL GetNewTabPageURL(Profile* profile) {
  return NewTabURLDetails::ForProfile(profile).url;
}

bool HandleNewTabURLRewrite(GURL* url,
                            content::BrowserContext* browser_context) {
  if (!IsInstantExtendedAPIEnabled()) {
    return false;
  }

  if (!(url->SchemeIs(content::kChromeUIScheme) &&
        url->host() == chrome::kChromeUINewTabHost)) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  NewTabURLDetails details(NewTabURLDetails::ForProfile(profile));
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.URLState", details.state,
                            NEW_TAB_URL_MAX);
  if (details.url.is_valid()) {
    *url = details.url;
    return true;
  }
  return false;
}

bool HandleNewTabURLReverseRewrite(GURL* url,
                                   content::BrowserContext* browser_context) {
  if (!IsInstantExtendedAPIEnabled()) {
    return false;
  }

  // Do nothing in incognito.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);
  if (profile->IsOffTheRecord()) {
    return false;
  }

  if (IsNTPOrRelatedURL(*url, profile)) {
    *url = chrome::ChromeUINewTabURLAsGURL();
    return true;
  }

  return false;
}

}  // namespace search
