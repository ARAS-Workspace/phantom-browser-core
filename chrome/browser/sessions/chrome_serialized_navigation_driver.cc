// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/chrome_serialized_navigation_driver.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/common/referrer.h"

namespace {}  // namespace

ChromeSerializedNavigationDriver::~ChromeSerializedNavigationDriver() = default;

// static
ChromeSerializedNavigationDriver*
ChromeSerializedNavigationDriver::GetInstance() {
  return base::Singleton<
      ChromeSerializedNavigationDriver,
      base::LeakySingletonTraits<ChromeSerializedNavigationDriver>>::get();
}

void ChromeSerializedNavigationDriver::Sanitize(
    sessions::SerializedNavigationEntry* navigation) const {
  content::Referrer old_referrer(
      navigation->referrer_url(),
      content::Referrer::ConvertToPolicy(navigation->referrer_policy()));
  content::Referrer new_referrer = content::Referrer::SanitizeForRequest(
      navigation->virtual_url(), old_referrer);

  // No need to compare the policy, as it doesn't change during
  // sanitization. If there has been a change, the referrer needs to be
  // stripped from the page state as well.
  if (navigation->referrer_url() != new_referrer.url) {
    auto* driver = sessions::SerializedNavigationDriver::Get();
    navigation->set_referrer_url(GURL());
    navigation->set_referrer_policy(driver->GetDefaultReferrerPolicy());
    navigation->set_encoded_page_state(
        driver->StripReferrerFromPageState(navigation->encoded_page_state()));
  }
}

ChromeSerializedNavigationDriver::ChromeSerializedNavigationDriver() = default;
