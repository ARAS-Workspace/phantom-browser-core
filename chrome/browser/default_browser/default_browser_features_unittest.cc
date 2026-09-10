// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_features.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

TEST(DefaultBrowserFeaturesTest, IsDefaultBrowserPromptSurfacesEnabled) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kDefaultBrowserPromptSurfaces);
    EXPECT_FALSE(IsDefaultBrowserPromptSurfacesEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kDefaultBrowserPromptSurfaces);
    EXPECT_FALSE(IsDefaultBrowserPromptSurfacesEnabled());
  }
}

TEST(DefaultBrowserFeaturesTest, IsDefaultBrowserChangedOsNotificationEnabled) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kDefaultBrowserChangedOsNotification);
    EXPECT_FALSE(IsDefaultBrowserChangedOsNotificationEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kDefaultBrowserChangedOsNotification);
    EXPECT_FALSE(IsDefaultBrowserChangedOsNotificationEnabled());
  }
}

// Tests for GetDefaultBrowserPromptSurface behavior with prompt surfaces and
// setter selection.
TEST(DefaultBrowserFeaturesTest, GetDefaultBrowserPromptSurface) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserPromptSurfaces,
          {{"prompt_surface", "modal_dialog_with_settings_illustration"}}},
         {kDefaultBrowserSetterSelection, {{"setter_option", "visual_guide"}}}},
        {});
    EXPECT_EQ(GetDefaultBrowserPromptSurface(),
              DefaultBrowserPromptSurface::kInfobar);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserPromptSurfaces,
          {{"prompt_surface", "modal_dialog_with_settings_illustration"}}},
         {kDefaultBrowserSetterSelection,
          {{"setter_option", "shell_integration"}}}},
        {});
    EXPECT_EQ(GetDefaultBrowserPromptSurface(),
              DefaultBrowserPromptSurface::kInfobar);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserPromptSurfaces,
          {{"prompt_surface", "modal_dialog_without_settings_illustration"}}}},
        {});
    EXPECT_EQ(GetDefaultBrowserPromptSurface(),
              DefaultBrowserPromptSurface::kInfobar);
  }
}

}  // namespace default_browser
