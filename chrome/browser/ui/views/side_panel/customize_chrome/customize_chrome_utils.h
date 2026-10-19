// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_UTILS_H_

namespace content {
class BrowserContext;
}

namespace customize_chrome {

// Disables the current NTP extension for |browser_context| if there is one.
void MaybeDisableExtensionOverridingNtp(
    content::BrowserContext* browser_context);

}  // namespace customize_chrome

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_UTILS_H_
