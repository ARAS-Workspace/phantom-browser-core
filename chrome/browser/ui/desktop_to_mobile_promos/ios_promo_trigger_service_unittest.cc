// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promo_trigger_service.h"

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/desktop_to_mobile_promos/features.h"
#include "components/desktop_to_mobile_promos/pref_names.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using desktop_to_mobile_promos::PromoType;
using testing::_;

std::unique_ptr<KeyedService> BuildFakeDeviceInfoSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::FakeDeviceInfoSyncService>();
}


}  // namespace

class IOSPromoTriggerServiceTest : public testing::Test {
 public:
  IOSPromoTriggerServiceTest() = default;
  ~IOSPromoTriggerServiceTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kMobilePromoOnDesktopWithReminder,
        {{kMobilePromoOnDesktopNotificationParam, "true"}});

    TestingProfile::Builder builder;
    profile_ = builder.Build();

    DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildFakeDeviceInfoSyncService));

    service_ = std::make_unique<IOSPromoTriggerService>(profile_.get());

    syncer::FakeDeviceInfoSyncService* fake_device_service =
        static_cast<syncer::FakeDeviceInfoSyncService*>(
            DeviceInfoSyncServiceFactory::GetForProfile(profile_.get()));

    fake_device_info_tracker_ = static_cast<syncer::FakeDeviceInfoTracker*>(
        fake_device_service->GetDeviceInfoTracker());
  }

 protected:
  syncer::DeviceInfo* AddDevice(const std::string& guid,
                                syncer::DeviceInfo::OsType os_type,
                                base::Time last_updated,
                                std::optional<syncer::DeviceInfo::FormFactor>
                                    form_factor = std::nullopt) {
    syncer::TestDeviceInfoBuilder builder(os_type);
    builder.WithGuid(guid).WithLastUpdatedTimestamp(last_updated);
    if (form_factor) {
      builder.WithFormFactor(*form_factor);
    }
    auto device = builder.Build();
    syncer::DeviceInfo* device_ptr = device.get();
    fake_device_info_tracker_->Add(std::move(device));
    return device_ptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IOSPromoTriggerService> service_;
  raw_ptr<syncer::FakeDeviceInfoTracker> fake_device_info_tracker_;
};

TEST_F(IOSPromoTriggerServiceTest, NotifiesCallback) {
  int call_count = 0;
  std::optional<PromoType> promo_type;

  base::CallbackListSubscription subscription =
      service_->RegisterPromoCallback(base::BindRepeating(
          [](int* call_count, std::optional<PromoType>* promo_type,
             PromoType type) {
            (*call_count)++;
            *promo_type = type;
          },
          &call_count, &promo_type));

  EXPECT_EQ(call_count, 0);
  EXPECT_FALSE(promo_type.has_value());

  service_->NotifyPromoShouldBeShown(PromoType::kPassword);

  EXPECT_EQ(call_count, 1);
  ASSERT_TRUE(promo_type.has_value());
  EXPECT_EQ(promo_type.value(), PromoType::kPassword);
}

TEST_F(IOSPromoTriggerServiceTest,
       CallbackIsRemovedWhenSubscriptionIsDestroyed) {
  int call_count = 0;
  std::optional<PromoType> promo_type;

  {
    base::CallbackListSubscription subscription =
        service_->RegisterPromoCallback(base::BindRepeating(
            [](int* call_count, std::optional<PromoType>* promo_type,
               PromoType type) {
              (*call_count)++;
              *promo_type = type;
            },
            &call_count, &promo_type));
  }

  service_->NotifyPromoShouldBeShown(PromoType::kPassword);

  EXPECT_EQ(call_count, 0);
}

TEST_F(IOSPromoTriggerServiceTest, GetIOSDeviceToRemind_NoDevices) {
  EXPECT_EQ(nullptr, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest, GetIOSDeviceToRemind_NoIOSDevices) {
  AddDevice("guid1", syncer::DeviceInfo::OsType::kAndroid, base::Time::Now());
  AddDevice("guid2", syncer::DeviceInfo::OsType::kWindows, base::Time::Now());

  EXPECT_EQ(nullptr, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest, GetIOSDeviceToRemind_SingleiPhone) {
  const syncer::DeviceInfo* iphone =
      AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS, base::Time::Now());

  EXPECT_EQ(iphone, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest, GetIOSDeviceToRemind_SingleiPad) {
  const syncer::DeviceInfo* ipad =
      AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS, base::Time::Now(),
                syncer::DeviceInfo::FormFactor::kTablet);

  EXPECT_EQ(ipad, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest,
       GetIOSDeviceToRemind_MultipleiPhonesMostRecent) {
  AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS,
            base::Time::Now() - base::Minutes(10));
  const syncer::DeviceInfo* iphone2 =
      AddDevice("guid2", syncer::DeviceInfo::OsType::kIOS, base::Time::Now());

  EXPECT_EQ(iphone2, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest,
       GetIOSDeviceToRemind_MultipleiPadsMostRecent) {
  AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS,
            base::Time::Now() - base::Minutes(10),
            syncer::DeviceInfo::FormFactor::kTablet);
  const syncer::DeviceInfo* ipad2 =
      AddDevice("guid2", syncer::DeviceInfo::OsType::kIOS, base::Time::Now(),
                syncer::DeviceInfo::FormFactor::kTablet);

  EXPECT_EQ(ipad2, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest,
       GetIOSDeviceToRemind_iPhonePreferredOveriPad) {
  const syncer::DeviceInfo* old_iphone =
      AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS,
                base::Time::Now() - base::Days(1));
  AddDevice("guid2", syncer::DeviceInfo::OsType::kIOS, base::Time::Now(),
            syncer::DeviceInfo::FormFactor::kTablet);

  EXPECT_EQ(old_iphone, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest,
       GetIOSDeviceToRemind_iPhonePreferredOveriPadMostRecent) {
  const syncer::DeviceInfo* recent_iphone =
      AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS, base::Time::Now());
  AddDevice("guid2", syncer::DeviceInfo::OsType::kIOS,
            base::Time::Now() - base::Days(1),
            syncer::DeviceInfo::FormFactor::kTablet);

  EXPECT_EQ(recent_iphone, service_->GetIOSDeviceToRemind());
}
