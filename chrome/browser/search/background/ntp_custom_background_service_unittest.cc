// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_custom_background_service.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/scoped_observation.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/themes/ntp_custom_background_service_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class MockNtpCustomBackgroundServiceObserver
    : public NtpCustomBackgroundServiceObserver {
 public:
  MOCK_METHOD0(OnCustomBackgroundImageUpdated, void());
};

std::unique_ptr<TestingProfile> MakeTestingProfile() {
  TestingProfile::Builder profile_builder;
  auto profile = profile_builder.Build();
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile.get(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  return profile;
}

base::DictValue GetBackgroundInfoAsDict(const GURL& background_url,
                                        const GURL& thumbnail_url) {
  base::DictValue background_info;
  background_info.Set("background_url", base::Value(background_url.spec()));
  background_info.Set("thumbnail_url", base::Value(thumbnail_url.spec()));
  background_info.Set("attribution_line_1", base::Value(std::string()));
  background_info.Set("attribution_line_2", base::Value(std::string()));
  background_info.Set("attribution_action_url", base::Value(std::string()));
  background_info.Set("collection_id", base::Value(std::string()));
  background_info.Set("resume_token", base::Value(std::string()));
  background_info.Set("refresh_timestamp", base::Value(0));
  return background_info;
}

}  // namespace

class NtpCustomBackgroundServiceTest : public testing::Test {
 public:
  NtpCustomBackgroundServiceTest() : profile_(MakeTestingProfile()) {}

  void SetUp() override {
    custom_background_service_ =
        NtpCustomBackgroundServiceFactory::GetForProfile(profile_.get());
    scoped_observation_.Observe(custom_background_service_);
  }

  void TearDown() override { scoped_observation_.Reset(); }

  TestingProfile& profile() { return *profile_; }

 protected:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  MockNtpCustomBackgroundServiceObserver observer_;
  raw_ptr<NtpCustomBackgroundService> custom_background_service_;
  base::ScopedObservation<NtpCustomBackgroundService,
                          NtpCustomBackgroundServiceObserver>
      scoped_observation_{&observer_};
};

TEST_F(NtpCustomBackgroundServiceTest, ResetRemovesLocalBackgroundImageCopy) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(1);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile().GetTestingPrefService();

  base::FilePath profile_path = profile().GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename));
  base::WriteFile(path, "background_image");
  pref_service->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, true);
  ASSERT_TRUE(custom_background_service_->IsCustomBackgroundSet());
  auto custom_background = custom_background_service_->GetCustomBackground();
  ASSERT_TRUE(custom_background.has_value());
  EXPECT_FALSE(custom_background->custom_background_main_color.has_value());

  custom_background_service_->ResetCustomBackgroundInfo();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(path));
  EXPECT_FALSE(
      pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  EXPECT_FALSE(custom_background_service_->GetCustomBackground().has_value());
}

TEST_F(NtpCustomBackgroundServiceTest, UpdatingPrefUpdatesNtpTheme) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(2);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kUrlFoo("https://www.foo.com");
  const GURL kUrlBar("https://www.bar.com");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile().GetTestingPrefService();
  pref_service->SetUserPref(prefs::kNtpCustomBackgroundDict,
                            GetBackgroundInfoAsDict(kUrlFoo, GURL()));

  EXPECT_TRUE(custom_background_service_->GetCustomBackground().has_value());
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  pref_service->SetUserPref(prefs::kNtpCustomBackgroundDict,
                            GetBackgroundInfoAsDict(kUrlBar, GURL()));

  EXPECT_TRUE(custom_background_service_->GetCustomBackground().has_value());
  EXPECT_EQ(false,
            pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest, SyncPrefOverridesAndRemovesLocalImage) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(1);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com/");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile().GetTestingPrefService();

  base::FilePath profile_path = profile().GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename));
  base::WriteFile(path, "background_image");
  base::ThreadPoolInstance::Get()->FlushForTesting();

  pref_service->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, true);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(
      pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(base::PathExists(path));

  // Update custom_background info via Sync.
  pref_service->SetUserPref(prefs::kNtpCustomBackgroundDict,
                            GetBackgroundInfoAsDict(kUrl, GURL()));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(custom_background_service_->GetCustomBackground().has_value());
  EXPECT_FALSE(
      pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_FALSE(base::PathExists(path));
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());
}
