// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/ntp_custom_background_service_base.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/themes/ntp_custom_background_service_constants.h"
#include "components/themes/ntp_custom_background_service_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestDictPref[] = "dict_pref";
const char kTestLocalPref[] = "local_pref";
const char kTestBackgroundUrlKey[] = "background_url";
const char kTestBackgroundUrl[] = "https://example.com/image.jpg";
const uint32_t kTestMainColor = 0x123456;
}  // namespace

class MockNtpCustomBackgroundServiceObserver
    : public NtpCustomBackgroundServiceObserver {
 public:
  MOCK_METHOD(void, OnCustomBackgroundImageUpdated, (), (override));
  MOCK_METHOD(void, OnNtpCustomBackgroundServiceShuttingDown, (), (override));
};

class TestNtpCustomBackgroundServiceBase
    : public NtpCustomBackgroundServiceBase {
 public:
  explicit TestNtpCustomBackgroundServiceBase(PrefService* pref_service)
      : NtpCustomBackgroundServiceBase(pref_service,
                                       kTestDictPref,
                                       kTestLocalPref) {}

  using NtpCustomBackgroundServiceBase::IsCustomBackgroundPrefValid;
  using NtpCustomBackgroundServiceBase::NotifyAboutBackgrounds;
};

class NtpCustomBackgroundServiceBaseTest : public testing::Test {
 public:
  NtpCustomBackgroundServiceBaseTest() = default;

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterDictionaryPref(kTestDictPref);
    pref_service_->registry()->RegisterBooleanPref(kTestLocalPref, false);

    service_ = std::make_unique<TestNtpCustomBackgroundServiceBase>(
        pref_service_.get());
  }

  void TearDown() override {
    service_.reset();
    pref_service_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestNtpCustomBackgroundServiceBase> service_;
};

TEST_F(NtpCustomBackgroundServiceBaseTest, Initialization) {
  EXPECT_NE(service_, nullptr);
}

TEST_F(NtpCustomBackgroundServiceBaseTest, Observers) {
  testing::StrictMock<MockNtpCustomBackgroundServiceObserver> observer;
  service_->AddObserver(&observer);

  EXPECT_CALL(observer, OnCustomBackgroundImageUpdated());
  service_->NotifyAboutBackgrounds();

  service_->RemoveObserver(&observer);
  // StrictMock will fail if the observer is still notified.
  service_->NotifyAboutBackgrounds();
}

TEST_F(NtpCustomBackgroundServiceBaseTest, ResetCustomBackgroundInfo) {
  pref_service_->SetBoolean(kTestLocalPref, true);
  base::DictValue dict;
  dict.Set(kTestBackgroundUrlKey, base::Value(kTestBackgroundUrl));
  pref_service_->SetDict(kTestDictPref, std::move(dict));

  service_->ResetCustomBackgroundInfo();

  EXPECT_FALSE(pref_service_->GetBoolean(kTestLocalPref));
  EXPECT_TRUE(pref_service_->GetDict(kTestDictPref).empty());
}

TEST_F(NtpCustomBackgroundServiceBaseTest, IsCustomBackgroundPrefValid) {
  EXPECT_FALSE(service_->IsCustomBackgroundPrefValid());

  base::DictValue dict;
  dict.Set(kTestBackgroundUrlKey, base::Value(kTestBackgroundUrl));
  pref_service_->SetDict(kTestDictPref, std::move(dict));

  EXPECT_TRUE(service_->IsCustomBackgroundPrefValid());
}

TEST_F(NtpCustomBackgroundServiceBaseTest, UpdateBackgroundFromSync) {
  pref_service_->SetBoolean(kTestLocalPref, true);
  testing::StrictMock<MockNtpCustomBackgroundServiceObserver> observer;
  service_->AddObserver(&observer);

  EXPECT_CALL(observer, OnCustomBackgroundImageUpdated());
  service_->UpdateBackgroundFromSync();

  EXPECT_FALSE(pref_service_->GetBoolean(kTestLocalPref));

  service_->RemoveObserver(&observer);
}

TEST_F(NtpCustomBackgroundServiceBaseTest, GetCustomBackground) {
  base::DictValue dict;
  dict.Set(kNtpCustomBackgroundURL, base::Value(kTestBackgroundUrl));
  dict.Set(kNtpCustomBackgroundMainColor,
           base::Value(static_cast<int>(kTestMainColor)));
  pref_service_->SetDict(kTestDictPref, std::move(dict));

  std::optional<CustomBackground> background = service_->GetCustomBackground();
  ASSERT_TRUE(background.has_value());
  EXPECT_EQ(background->custom_background_main_color, kTestMainColor);

  // If local pref is true, it returns nullopt
  pref_service_->SetBoolean(kTestLocalPref, true);
  EXPECT_FALSE(service_->GetCustomBackground().has_value());
}
