// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_utils.h"

#include <memory>
#include <ranges>

#include "ash/constants/web_app_id_constants.h"
#include "base/files/file_path.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/model/web_app_icon_types.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/common/chrome_constants.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace web_app {

using WebAppUtilsTest = WebAppTest;
using ::testing::ElementsAre;

// Sanity check that iteration order of SortedSizesPx is ascending. The
// correctness of most usage of SortedSizesPx depends on this.
TEST(WebAppTest, SortedSizesPxIsAscending) {
  // Removal of duplicates is expected but not required for correctness.
  std::vector<SquareSizePx> in{512, 512, 16, 512, 64, 32, 256};
  SortedSizesPx sorted(in);
  ASSERT_THAT(sorted, ElementsAre(16, 32, 64, 256, 512));

  std::vector<SquareSizePx> out(sorted.begin(), sorted.end());
  ASSERT_THAT(out, ElementsAre(16, 32, 64, 256, 512));

  std::vector<SquareSizePx> reversed(sorted.rbegin(), sorted.rend());
  ASSERT_THAT(reversed, ElementsAre(512, 256, 64, 32, 16));

  std::vector<SquareSizePx> base_reversed(std::views::reverse(sorted).begin(),
                                          std::views::reverse(sorted).end());
  ASSERT_THAT(base_reversed, ElementsAre(512, 256, 64, 32, 16));
}

TEST_F(WebAppUtilsTest, AreWebAppsEnabled) {
  Profile* regular_profile = profile();

  EXPECT_FALSE(AreWebAppsEnabled(nullptr));
  EXPECT_TRUE(AreWebAppsEnabled(regular_profile));
  EXPECT_FALSE(AreWebAppsEnabled(
      regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  EXPECT_FALSE(AreWebAppsEnabled(regular_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true)));

  Profile* guest_profile = profile_manager().CreateGuestProfile();
  EXPECT_TRUE(AreWebAppsEnabled(guest_profile));
  EXPECT_TRUE(AreWebAppsEnabled(guest_profile->GetOriginalProfile()));
  EXPECT_FALSE(AreWebAppsEnabled(
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

}

TEST_F(WebAppUtilsTest, TransformFileExtensionsForDisplay_StripsBidiControls) {
  std::set<std::string> extensions = {
      ".aa\xE2\x80\x8E",  // LRM (Format)
      ".bb\xE2\x80\xAE",  // RLO (Format)
      ".cc\x01",          // SOH (Control)
      ".dd\x7F",          // DEL (Control)
      ".ee\xC2\x9F",      // APC (Control)
      ".bat",             // Safe
      ""                  // Empty
  };
  std::vector<std::u16string> transformed =
      TransformFileExtensionsForDisplay(extensions);

  EXPECT_THAT(transformed, ::testing::UnorderedElementsAre(
                               u"", u"AA", u"BB", u"CC", u"DD", u"EE", u"BAT"));
}

TEST_F(WebAppUtilsTest, AreWebAppsUserInstallable) {
  Profile* regular_profile = profile();

  EXPECT_FALSE(AreWebAppsEnabled(nullptr));
  EXPECT_TRUE(AreWebAppsUserInstallable(regular_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(regular_profile->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true)));

  Profile* guest_profile = profile_manager().CreateGuestProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(guest_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  Profile* system_profile = profile_manager().CreateSystemProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(system_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      system_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

}

TEST_F(WebAppUtilsTest, GetBrowserContextForWebApps) {
  Profile* regular_profile = profile();

  Profile* expected_otr_browser_context = nullptr;

  EXPECT_EQ(regular_profile, GetBrowserContextForWebApps(regular_profile));
  EXPECT_EQ(expected_otr_browser_context,
            GetBrowserContextForWebApps(regular_profile->GetPrimaryOTRProfile(
                /*create_if_needed=*/true)));
  EXPECT_EQ(expected_otr_browser_context,
            GetBrowserContextForWebApps(regular_profile->GetOffTheRecordProfile(
                Profile::OTRProfileID::CreateUniqueForTesting(),
                /*create_if_needed=*/true)));

  Profile* guest_profile = profile_manager().CreateGuestProfile();
  Profile* guest_otr_profile = guest_profile->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
  EXPECT_EQ(guest_profile, GetBrowserContextForWebApps(guest_profile));
  EXPECT_EQ(nullptr, GetBrowserContextForWebApps(guest_otr_profile));

  Profile* system_profile = profile_manager().CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebApps(system_profile));
  EXPECT_EQ(nullptr,
            GetBrowserContextForWebApps(system_profile->GetPrimaryOTRProfile(
                /*create_if_needed=*/true)));
}

TEST_F(WebAppUtilsTest, GetBrowserContextForWebAppMetrics) {
  Profile* regular_profile = profile();

  Profile* expected_otr_browser_context = nullptr;

  EXPECT_EQ(regular_profile,
            GetBrowserContextForWebAppMetrics(regular_profile));
  EXPECT_EQ(
      expected_otr_browser_context,
      GetBrowserContextForWebAppMetrics(
          regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  EXPECT_EQ(
      expected_otr_browser_context,
      GetBrowserContextForWebAppMetrics(regular_profile->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true)));

  Profile* guest_profile = profile_manager().CreateGuestProfile();
  Profile* guest_otr_profile = guest_profile->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(guest_profile));
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(guest_otr_profile));

  Profile* system_profile = profile_manager().CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(system_profile));
  EXPECT_EQ(
      nullptr,
      GetBrowserContextForWebAppMetrics(
          system_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}



}  // namespace web_app
