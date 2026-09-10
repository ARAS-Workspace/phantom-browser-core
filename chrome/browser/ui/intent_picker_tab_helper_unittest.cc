// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intent_picker_tab_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/intent_helper/intent_chip_display_prefs.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"

class IntentPickerTabHelperTest : public ChromeRenderViewHostTestHarness,
                                  public testing::WithParamInterface<
                                      apps::test::LinkCapturingFeatureVersion> {
 public:
  IntentPickerTabHelperTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(GetParam()), {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    IntentPickerTabHelper::CreateForWebContents(web_contents());
    helper_ = IntentPickerTabHelper::FromWebContents(web_contents());
  }

  IntentPickerTabHelper* helper() { return helper_; }

  std::vector<apps::IntentPickerAppInfo> CreateTestAppList() {
    return {
        {apps::PickerEntryType::kWeb, ui::ImageModel(), "app_id", "Test app"}};
  }

 private:
  raw_ptr<IntentPickerTabHelper, DanglingUntriaged> helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(IntentPickerTabHelperTest, ShowOrHideIcon) {
  IntentPickerTabHelper::ShowOrHideIcon(web_contents(),
                                        /*should_show_icon=*/true);

  ASSERT_TRUE(helper()->should_show_icon());

  IntentPickerTabHelper::ShowOrHideIcon(web_contents(),
                                        /*should_show_icon=*/false);

  ASSERT_FALSE(helper()->should_show_icon());
}

TEST_P(IntentPickerTabHelperTest, ShowIconForApps) {
  NavigateAndCommit(GURL("https://www.google.com"));
  helper()->MaybeShowIconForApps(CreateTestAppList());

  ASSERT_TRUE(helper()->should_show_icon());
}

TEST_P(IntentPickerTabHelperTest, ShowIconForApps_ExpandedChip) {
  const GURL kTestUrl = GURL("https://www.google.com");

  NavigateAndCommit(kTestUrl);
  helper()->MaybeShowIconForApps(CreateTestAppList());

  ASSERT_TRUE(helper()->ShouldShowExpandedChip());
}

TEST_P(IntentPickerTabHelperTest, ShowIconForApps_CollapsedChip) {
  const GURL kTestUrl = GURL("https://www.google.com");

  // Simulate having seen the chip for this URL several times before, so that it
  // appears collapsed.
  for (int i = 0; i < 3; i++) {
    IntentChipDisplayPrefs::GetChipStateAndIncrementCounter(profile(),
                                                            kTestUrl);
  }

  NavigateAndCommit(kTestUrl);
  helper()->MaybeShowIconForApps(CreateTestAppList());

  ASSERT_TRUE(helper()->should_show_icon());
  ASSERT_FALSE(helper()->ShouldShowExpandedChip());
}

TEST_P(IntentPickerTabHelperTest, ShowIntentIcon_ResetsExpandedState) {
  const GURL kTestUrl = GURL("https://www.google.com");

  NavigateAndCommit(kTestUrl);
  helper()->MaybeShowIconForApps(CreateTestAppList());

  EXPECT_TRUE(helper()->should_show_icon());
  EXPECT_TRUE(helper()->ShouldShowExpandedChip());

  // Explicitly showing the icon should reset any app-based customizations.
  IntentPickerTabHelper::ShowOrHideIcon(web_contents(),
                                        /*should_show_icon=*/true);
  ASSERT_FALSE(helper()->ShouldShowExpandedChip());
}

TEST_P(IntentPickerTabHelperTest, IconShownMetricsTriggered) {
  base::HistogramTester histogram_tester;

  NavigateAndCommit(GURL("https://www.google.com"));

  // Create empty app list which ensures the intent picker icon is hidden.
  helper()->MaybeShowIconForApps({});
  histogram_tester.ExpectBucketCount(
      "Webapp.Site.Intents.IntentPickerIconEvent",
      apps::IntentPickerIconEvent::kIconShown, 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerTabHelperTest,
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOn),
    apps::test::LinkCapturingVersionToString);
