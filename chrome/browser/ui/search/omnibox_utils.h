// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_OMNIBOX_UTILS_H_
#define CHROME_BROWSER_UI_SEARCH_OMNIBOX_UTILS_H_

class OmniboxController;
namespace content {
class WebContents;
}  // namespace content

namespace search {

// Returns the omnibox controller from the browser instance associated with
// `web_contents`, if any, or nullptr otherwise.
OmniboxController* GetOmniboxController(content::WebContents* web_contents);

}  // namespace search

#endif  // CHROME_BROWSER_UI_SEARCH_OMNIBOX_UTILS_H_
