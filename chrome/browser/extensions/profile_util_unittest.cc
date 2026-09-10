// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/profile_util.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service_user_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using extensions::profile_util::ProfileCanUseNonComponentExtensions;

namespace extensions {

class ProfileUtilUnitTest : public ExtensionServiceUserTestBase {
 public:
  void SetUp() override {
    ExtensionServiceUserTestBase::SetUp();
    InitializeEmptyExtensionService();
  }
};

TEST_F(ProfileUtilUnitTest,
       ProfileCanUseNonComponentExtensions_RegularProfile) {
  // profile() defaults to a regular profile.
  EXPECT_TRUE(ProfileCanUseNonComponentExtensions(profile()));
}

TEST_F(ProfileUtilUnitTest,
       Browser_ProfileCannotUseNonComponentExtensions_NoProfile) {
  EXPECT_FALSE(ProfileCanUseNonComponentExtensions(/*profile=*/nullptr));
}

TEST_F(ProfileUtilUnitTest,
       ProfileCannotUseNonComponentExtensions_GuestProfile) {
  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(/*is_guest=*/true));
  EXPECT_FALSE(ProfileCanUseNonComponentExtensions(profile()));
}

TEST_F(ProfileUtilUnitTest,
       Browser_ProfileCannotUseNonComponentExtensions_IncognitoProfile) {
  auto* incognito_test_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(incognito_test_profile->IsIncognitoProfile());
  EXPECT_FALSE(ProfileCanUseNonComponentExtensions(incognito_test_profile));
}

}  // namespace extensions
