// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/survey_config.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"

class SurveyConfigTest : public testing::Test {
 public:
  SurveyConfigTest() = default;
};

TEST_F(SurveyConfigTest, ValidateHatsHistogramName) {
  EXPECT_EQ(std::nullopt,
            hats::SurveyConfig::ValidateHatsHistogramName(std::nullopt));
  EXPECT_EQ(std::nullopt, hats::SurveyConfig::ValidateHatsHistogramName(""));
  EXPECT_EQ(std::nullopt,
            hats::SurveyConfig::ValidateHatsHistogramName("ExampleSurvey"));

  EXPECT_EQ(std::make_optional<std::string>(
                "Feedback.HappinessTrackingSurvey.ExampleSurvey"),
            hats::SurveyConfig::ValidateHatsHistogramName(
                "Feedback.HappinessTrackingSurvey.ExampleSurvey"));
}

TEST_F(SurveyConfigTest, ValidateHatsSurveyUkmId) {
  EXPECT_EQ(std::nullopt, hats::SurveyConfig::ValidateHatsSurveyUkmId(0));
  EXPECT_EQ(std::nullopt,
            hats::SurveyConfig::ValidateHatsSurveyUkmId(std::nullopt));

  EXPECT_EQ(std::make_optional<uint64_t>(1),
            hats::SurveyConfig::ValidateHatsSurveyUkmId(1));
}

// The Trust & Safety V2 surveys stay registered with their trigger ids.
TEST_F(SurveyConfigTest, TrustSafetyV2ConfigsActive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTrustSafetySentimentSurveyV2);
  hats::SurveyConfigs survey_configs;
  hats::GetActiveSurveyConfigs(survey_configs);

  const struct {
    const char* trigger;
    std::string trigger_id;
  } kExpected[] = {
      {kHatsSurveyTriggerTrustSafetyV2ControlGroup,
       features::kTrustSafetySentimentSurveyV2ControlGroupTriggerId.Get()},
      {kHatsSurveyTriggerTrustSafetyV2PasswordCheck,
       features::kTrustSafetySentimentSurveyV2PasswordCheckTriggerId.Get()},
      {kHatsSurveyTriggerTrustSafetyV2PrivacyGuide,
       features::kTrustSafetySentimentSurveyV2PrivacyGuideTriggerId.Get()},
  };
  for (const auto& expected : kExpected) {
    SCOPED_TRACE(expected.trigger);
    ASSERT_TRUE(survey_configs.contains(expected.trigger));
    EXPECT_EQ(expected.trigger_id,
              survey_configs.at(expected.trigger).trigger_id);
  }
  EXPECT_FALSE(survey_configs.contains("ts-v2-download-warning-ui"));
}
