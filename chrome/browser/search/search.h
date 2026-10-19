// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_H_
#define CHROME_BROWSER_SEARCH_SEARCH_H_

#include "build/build_config.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
}

namespace search {

// Returns whether Google is selected as the default search engine.
bool DefaultSearchProviderIsGoogle(Profile* profile);

// Returns true if |url| corresponds to a New Tab page.
bool IsNTPOrRelatedURL(const GURL& url, Profile* profile);

// Returns whether a |url| corresponds to a New Tab page.
bool IsNTPURL(const GURL& url);

// Returns whether the Web UI NTP is enabled for Desktop Android. In all other
// cases and platforms, it returns false.
bool IsWebUiNtpEnabledForDesktopAndroid();

// Returns true if |url| corresponds to a Split View New Tab page.
bool IsSplitViewNewTabPage(const GURL& url);

// Returns the New Tab page URL for the given |profile|.
GURL GetNewTabPageURL(Profile* profile);

// Rewrites |url| to the actual NTP URL to use if
//   1. |url| is "chrome://newtab",
//   2. InstantExtended is enabled, and
//   3. |browser_context| doesn't correspond to an incognito profile.
// The actual NTP URL is chrome://new-tab-page-third-party.
bool HandleNewTabURLRewrite(GURL* url,
                            content::BrowserContext* browser_context);
// Reverses the operation from HandleNewTabURLRewrite.
bool HandleNewTabURLReverseRewrite(GURL* url,
                                   content::BrowserContext* browser_context);

}  // namespace search

#endif  // CHROME_BROWSER_SEARCH_SEARCH_H_
