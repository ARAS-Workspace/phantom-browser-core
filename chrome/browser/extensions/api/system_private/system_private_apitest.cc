// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, GetIncognitoModeAvailability) {
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetInteger(policy::policy_prefs::kIncognitoModeAvailability, 1);

  EXPECT_TRUE(RunExtensionTest("system/get_incognito_mode_availability", {},
                               {.load_as_component = true}))
      << message_;
}

}  // namespace extensions
