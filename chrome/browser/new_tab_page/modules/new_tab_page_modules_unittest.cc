// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/new_tab_page/modules/modules_constants.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/modules/test_support.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<TestingProfile> MakeTestingProfile(
    network::TestURLLoaderFactory& url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetSharedURLLoaderFactory(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &url_loader_factory));
  profile_builder.AddTestingFactory(
      ChromeSigninClientFactory::GetInstance(),
      base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                          &url_loader_factory));
  return IdentityTestEnvironmentProfileAdaptor::
      CreateProfileForIdentityTestEnvironment(profile_builder);
}

const char kSampleUserEmail[] = "user@gmail.com";
const std::vector<ntp::ModuleIdDetail>& GetSampleModules() {
  static const base::NoDestructor<std::vector<ntp::ModuleIdDetail>> modules(
      std::vector<ntp::ModuleIdDetail>{
          {ntp_modules::kTabGroupsModuleId, IDS_NTP_MODULES_TAB_GROUPS_TITLE}});
  return *modules;
}

}  // namespace

class NewTabPageModulesTest : public testing::Test {
 public:
  NewTabPageModulesTest() {
    profile_ = MakeTestingProfile(test_url_loader_factory_);
    identity_test_environment_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    identity_test_env().SetTestURLLoaderFactory(&test_url_loader_factory_);
  }

  TestingProfile& profile() { return *profile_; }
  signin::IdentityTestEnvironment& identity_test_env() {
    return *identity_test_environment_profile_adaptor_->identity_test_env();
  }

 private:
  // Must be on Chrome_UIThread.
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_profile_adaptor_;
};

TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_PopulatesStructCorrectly) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpMostRelevantTabResumptionModule},
      /*disabled_features=*/{});
  identity_test_env().SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});

  const std::vector<ntp::ModuleIdDetail> module_id_details =
      ntp::MakeModuleIdDetails(/*is_managed_profile=*/false,
                               /*profile=*/&profile());
  EXPECT_EQ(1u, module_id_details.size());
  const auto& tab_resumption_details = module_id_details[0];
  EXPECT_EQ(ntp_modules::kMostRelevantTabResumptionModuleId,
            tab_resumption_details.id_);
  EXPECT_EQ(IDS_NTP_MODULES_MOST_RELEVANT_TAB_RESUMPTION_TITLE,
            tab_resumption_details.name_message_id_);
  EXPECT_EQ(std::nullopt, tab_resumption_details.description_message_id_);
}

TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_OnlyPopulatesEnabledModules) {
  identity_test_env().SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  const std::vector<base::test::FeatureRef>& some_module_features = {
      ntp_features::kNtpTabGroupsModule,
      ntp_features::kNtpMostRelevantTabResumptionModule};
  for (auto& feature : some_module_features) {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{feature},
        /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
            some_module_features, {feature}));

    const std::vector<ntp::ModuleIdDetail> module_id_details =
        ntp::MakeModuleIdDetails(/*is_managed_profile=*/false,
                                 /*profile=*/&profile());
    ASSERT_EQ(1u, module_id_details.size());
  }
}

#if !defined(OFFICIAL_BUILD)
TEST_F(NewTabPageModulesTest, MakeModuleIdDetails_DummyModules) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpDummyModules},
      /*disabled_features=*/ntp::kAllModuleFeatures);

  const std::vector<ntp::ModuleIdDetail> module_id_details =
      ntp::MakeModuleIdDetails(/*is_managed_profile=*/false,
                               /*profile=*/&profile());
  ASSERT_EQ(1u, module_id_details.size());
}
#endif

TEST_F(NewTabPageModulesTest, HasModulesEnabled) {
  identity_test_env().SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  ASSERT_TRUE(ntp::HasModulesEnabled(GetSampleModules(),
                                     identity_test_env().identity_manager()));
}

TEST_F(NewTabPageModulesTest, HasModulesEnabled_NtpModulesLoadFlag) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpModulesLoad},
      /*disabled_features=*/{});

  identity_test_env().SetCookieAccounts(
      {{kSampleUserEmail, signin::GetTestGaiaIdForEmail(kSampleUserEmail)}});
  ASSERT_FALSE(ntp::HasModulesEnabled(GetSampleModules(),
                                      identity_test_env().identity_manager()));
}
