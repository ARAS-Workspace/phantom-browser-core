// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/omnibox_utils.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace search {

namespace {

LocationBar* GetLocationBar(content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  const auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return nullptr;
  }

  auto* bwi =
      const_cast<BrowserWindowInterface*>(tab->GetBrowserWindowInterface());
  if (!bwi) {
    return nullptr;
  }

  return bwi->GetFeatures().location_bar();
}

}  // namespace

OmniboxController* GetOmniboxController(content::WebContents* web_contents) {
  auto* location_bar = GetLocationBar(web_contents);
  if (!location_bar) {
    return nullptr;
  }

  return location_bar->GetOmniboxController();
}

}  // namespace search
