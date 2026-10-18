// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

// Per-tab helper that creates the OmniboxTabHelper, records which New Tab page
// a chrome://newtab navigation lands on and sets the New Tab title on pending
// New Tab page entries.
class SearchTabHelper : public content::WebContentsObserver,
                        public content::WebContentsUserData<SearchTabHelper> {
 public:
  SearchTabHelper(const SearchTabHelper&) = delete;
  SearchTabHelper& operator=(const SearchTabHelper&) = delete;

  ~SearchTabHelper() override;

 private:
  friend class content::WebContentsUserData<SearchTabHelper>;

  explicit SearchTabHelper(content::WebContents* web_contents);

  // Overridden from contents::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  Profile* profile() const;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
