// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_hats_delegate.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/ui/hats/hats_service_desktop.h"

namespace safe_browsing {

namespace {

class MockHatsService : public HatsServiceDesktop {
 public:
  explicit MockHatsService(Profile* profile) : HatsServiceDesktop(profile) {}
  ~MockHatsService() override = default;

  MOCK_METHOD(HatsService::LaunchError,
              LaunchSurvey,
              (const std::string& trigger,
               base::OnceClosure success_callback,
               base::OnceClosure failure_callback,
               const SurveyBitsData& product_specific_bits_data,
               const SurveyStringData& product_specific_string_data,
               const std::optional<std::string>& supplied_trigger_id,
               const SurveyOptions& survey_options),
              (override));
};

std::unique_ptr<KeyedService> BuildMockHatsService(
    content::BrowserContext* context) {
  return std::make_unique<MockHatsService>(
      Profile::FromBrowserContext(context));
}

}  // namespace

class ChromeSafeBrowsingHatsDelegateTest : public testing::Test {
 protected:
  ChromeSafeBrowsingHatsDelegateTest() {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, base::BindRepeating(&BuildMockHatsService)));
    delegate_ = std::make_unique<ChromeSafeBrowsingHatsDelegate>(&profile_);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
  std::unique_ptr<ChromeSafeBrowsingHatsDelegate> delegate_;
};

TEST_F(ChromeSafeBrowsingHatsDelegateTest, LaunchRedWarningSurvey) {
  std::map<std::string, std::string> product_specific_string_data = {
      {safe_browsing::kFlaggedUrl, "http://example.com"}};

  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurvey(
          kHatsSurveyTriggerRedWarning, testing::_, testing::_, testing::_,
          product_specific_string_data, testing::Eq(std::nullopt),
          testing::Field(&HatsService::SurveyOptions::custom_invitation,
                         testing::Eq(std::nullopt))));

  delegate_->LaunchRedWarningSurvey(product_specific_string_data,
                                    /*product_specific_bits_data=*/{});
}

TEST_F(ChromeSafeBrowsingHatsDelegateTest,
       LaunchRedWarningSurvey_FiltersDesktopPsd) {
  std::map<std::string, std::string> all_string_data = {
      {safe_browsing::kFlaggedUrl, "http://example.com"},
      {safe_browsing::kMainFrameUrl, "http://page.com"},
      {safe_browsing::kReferrerUrl, "http://ref.com"},
      {safe_browsing::kUserActivityWithUrls, "encoded_proto"},
      {safe_browsing::kUserAction, "PROCEED"},
      {safe_browsing::kReportType, "URL_CLIENT_SIDE_PHISHING"},
  };

  std::map<std::string, std::string> expected_desktop_data = {
      {safe_browsing::kFlaggedUrl, "http://example.com"},
      {safe_browsing::kMainFrameUrl, "http://page.com"},
      {safe_browsing::kReferrerUrl, "http://ref.com"},
      {safe_browsing::kUserActivityWithUrls, "encoded_proto"},
  };

  std::map<std::string, bool> bits_data = {
      {safe_browsing::kLearnMoreClicked, true},
  };

  EXPECT_CALL(*mock_hats_service_,
              LaunchSurvey(kHatsSurveyTriggerRedWarning, testing::_, testing::_,
                           testing::IsEmpty(), expected_desktop_data,
                           testing::Eq(std::nullopt), testing::_));

  delegate_->LaunchRedWarningSurvey(all_string_data, bits_data);
}

TEST_F(ChromeSafeBrowsingHatsDelegateTest,
       LaunchRedWarningSurvey_IncognitoProfileDelegateNoSurvey) {
  Profile* incognito_profile =
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto otr_delegate =
      std::make_unique<ChromeSafeBrowsingHatsDelegate>(incognito_profile);

  EXPECT_CALL(*mock_hats_service_, LaunchSurvey).Times(0);

  otr_delegate->LaunchRedWarningSurvey({}, /*product_specific_bits_data=*/{});
}

}  // namespace safe_browsing
