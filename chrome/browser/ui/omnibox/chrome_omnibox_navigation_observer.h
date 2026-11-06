// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_navigation_observer.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

// Monitors omnibox navigations in order to trigger behaviors that depend on
// successful navigations.
//
// Currently two such behaviors exist:
// (1) Omnibox navigations that complete successfully are added to the
//     Shortcuts backend.
// (2) Omnibox searches that result in a 404 for an auto-generated custom
//     search engine cause the custom search engine to be deleted.
//
// Please see the class comment on the base class for important information
// about the memory management of this object.
class ChromeOmniboxNavigationObserver
    : public base::RefCounted<ChromeOmniboxNavigationObserver>,
      public content::WebContentsObserver {
 public:
  static void Create(content::NavigationHandle* navigation,
                     Profile* profile,
                     const std::u16string& text,
                     const AutocompleteMatch& match);

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void On404();

 private:
  ChromeOmniboxNavigationObserver(content::NavigationHandle& navigation,
                                  Profile* profile,
                                  const std::u16string& text,
                                  const AutocompleteMatch& match);

  ~ChromeOmniboxNavigationObserver() override;

  friend class base::RefCounted<ChromeOmniboxNavigationObserver>;

  const std::u16string text_;
  const AutocompleteMatch match_;
  const int64_t navigation_id_;
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_H_
