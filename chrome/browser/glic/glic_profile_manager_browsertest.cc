// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include <memory>
#include <string>
#include <type_traits>

#include "base/byte_size.h"
#include "base/run_loop.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace glic {
namespace {

class GlicProfileManagerBrowserTest : public InProcessBrowserTest {
 public:
  GlicProfileManagerBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kDestroyProfileOnBrowserClose);

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &GlicProfileManagerBrowserTest::SetTestingFactory,
                base::Unretained(this)));

    // Manually set up these states with `SigninWithPrimaryAccount` and
    // `SetGlicCapability`.
    glic_test_environment_.SetForceSigninAndModelExecutionCapability(false);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Enable GLIC for the default profile.
    SigninWithPrimaryAccount(browser()->GetProfile());
    SetGlicCapability(browser()->GetProfile(), true);
  }

  MockGlicKeyedService* GetMockGlicKeyedService(Profile* profile) {
    auto* service = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
    return static_cast<MockGlicKeyedService*>(service);
  }

  // In ChromeOS, each regular profile is associated with a user session.
  Profile* CreateNewProfile(bool signin_and_allow_glic) {
    auto* profile_manager = g_browser_process->profile_manager();
    auto new_path = profile_manager->GenerateNextProfileDirectoryPath();
    profiles::testing::CreateProfileSync(profile_manager, new_path);
    Profile* new_profile = profile_manager->GetProfile(new_path);

    if (signin_and_allow_glic) {
      SigninWithPrimaryAccount(new_profile);
      SetGlicCapability(new_profile, true);
    }
    return new_profile;
  }

 protected:
  void SetTestingFactory(content::BrowserContext* context) {
    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &GlicProfileManagerBrowserTest::CreateMockGlicKeyedService,
                     base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateMockGlicKeyedService(
      content::BrowserContext* context) {
    auto* identitity_manager = IdentityManagerFactory::GetForProfile(
        Profile::FromBrowserContext(context));
    auto* actor_keyed_service =
        actor::ActorKeyedServiceFactory::GetActorKeyedService(context);
    return std::make_unique<MockGlicKeyedService>(
        context, identitity_manager, g_browser_process->profile_manager(),
        GlicProfileManager::GetInstance(),
        /*contextual_cueing_service=*/nullptr, actor_keyed_service);
  }

  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       ProfileForLaunch_WithDetachedGlic) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }

  auto* profile0 = browser()->GetProfile();
  auto* service0 = GetMockGlicKeyedService(profile0);

  // Setup Profile 1
  auto* profile1 =
      CreateNewProfile(/*signin_and_allow_glic=*/true);
  CHECK(profile1);

  auto* profile_manager = GlicProfileManager::GetInstance();
  // Profile 0 is the last used Glic and Profile 1 is the last used window.
  // Profile 1 should be selected for launch.
  CreateBrowser(profile1);
  EXPECT_EQ(profile1, profile_manager->GetProfileForLaunch());

  // Simulate showing detached for Profile 0.
  // Profile 0 should now be selected for launch.
  service0->SetWindowDetached(true);
  EXPECT_EQ(profile0, profile_manager->GetProfileForLaunch());
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       ProfileForLaunch_BasedOnActivationOrder) {
  auto* profile0 = browser()->GetProfile();
  ASSERT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile0));

  // Setup Profile 1
  auto* profile1 =
      CreateNewProfile(/*signin_and_allow_glic=*/true);
  ASSERT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile1));

  // Setup Profile 2 (not glic compliant)
  auto* profile2 =
      CreateNewProfile(/*signin_and_allow_glic=*/false);
  ASSERT_FALSE(GlicEnabling::IsEnabledAndConsentForProfile(profile2));

  auto* profile_manager = GlicProfileManager::GetInstance();
  // profile0 is the most recently used profile
  EXPECT_EQ(profile0, profile_manager->GetProfileForLaunch());

  // profile1 is the most recently used profile
  auto* browser1 = CreateBrowser(profile1);
  ui_test_utils::WaitForBrowserSetLastActive(browser1);
  EXPECT_EQ(profile1, profile_manager->GetProfileForLaunch());

  // profile2 is the most recently used profile but it isn't
  // compliant, so still using profile1
  auto* browser2 = CreateBrowser(profile2);
  ui_test_utils::WaitForBrowserSetLastActive(browser2);
  EXPECT_EQ(profile1, profile_manager->GetProfileForLaunch());

  bool is_wayland = false;
#if BUILDFLAG(IS_OZONE)
  is_wayland = ::ui::OzonePlatform::RunningOnWaylandForTest();
#endif
  if (!is_wayland) {
    // profile0 is the most recently used profile
    browser()->GetWindow()->Activate();
    ui_test_utils::WaitForBrowserSetLastActive(browser());
    EXPECT_EQ(profile0, profile_manager->GetProfileForLaunch());
  }
}

class GlicProfileManagerPreloadingTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  explicit GlicProfileManagerPreloadingTest(
      const std::string& delay_ms,
      const std::string& min_required_ram_mb = "0") {
    if (IsPrewarmingEnabled()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{{features::kGlicWarming,
                                 {{features::kGlicWarmingDelayMs.name,
                                   delay_ms},
                                  {features::kGlicWarmingJitterMs.name, "0"},
                                  {features::kGlicWarmingMinRequiredRamMb.name,
                                   min_required_ram_mb}}},
                                {features::
                                     kGlicAnchorEntryPointForOnboardedUsers,
                                 {}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlicAnchorEntryPointForOnboardedUsers},
          /*disabled_features=*/{features::kGlicWarming});
    }

    // We prevent any premature preloading by disabling it.
    GlicProfileManager::SetPrewarmingEnabledForTesting(false);
    GlicProfileManager::ForceConnectionTypeForTesting(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  }

  GlicProfileManagerPreloadingTest()
      : GlicProfileManagerPreloadingTest("0", "0") {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    GlicProfileManager::ForceProfileForLaunchForTesting(
        browser()->GetProfile());
  }

  void TearDown() override {
    GlicProfileManager::SetPrewarmingEnabledForTesting(true);
    GlicProfileManager::ForceProfileForLaunchForTesting(std::nullopt);
    GlicProfileManager::ForceConnectionTypeForTesting(std::nullopt);
    InProcessBrowserTest::TearDown();
  }

  bool IsPrewarmingEnabled() const { return GetParam(); }

  void ResetPrewarming() {
    GlicProfileManager::SetPrewarmingEnabledForTesting(true);
  }

  GlicPrewarmingChecksResult WaitForShouldPreload() {
    base::test::TestFuture<GlicPrewarmingChecksResult> future;
    GlicProfileManager::GetInstance()->ShouldPreloadForProfile(
        browser()->GetProfile(), future.GetCallback());
    return future.Get();
  }

  void SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType connection_type) {
    GlicProfileManager::ForceConnectionTypeForTesting(connection_type);
  }

  bool IsWarmed() {
    auto* service =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser()->GetProfile());
    return static_cast<GlicInstanceCoordinatorImpl&>(
               service->instance_coordinator())
        .GetWebContentsWarmingPoolForTesting()
        .HasWarmedContainerForTesting();
  }

 private:
  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_Success) {
  ResetPrewarming();
  const bool should_preload = IsPrewarmingEnabled();
  EXPECT_EQ(WaitForShouldPreload(),
            should_preload ? GlicPrewarmingChecksResult::kSuccess
                           : GlicPrewarmingChecksResult::kWarmingDisabled);
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_NotSupportedProfile) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  GlicProfileManager::ForceProfileForLaunchForTesting(std::nullopt);
  SetGlicCapability(browser()->GetProfile(), false);
  EXPECT_EQ(WaitForShouldPreload(),
            GlicPrewarmingChecksResult::kProfileNotEligibleAccountCapabilities);
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_WillBeDestroyed) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  browser()->GetProfile()->NotifyWillBeDestroyed();
  EXPECT_EQ(WaitForShouldPreload(),
            GlicPrewarmingChecksResult::kBrowserShuttingDown);
}

class GlicProfileManagerLowMemoryPreloadingTest
    : public GlicProfileManagerPreloadingTest {
 public:
  GlicProfileManagerLowMemoryPreloadingTest()
      : GlicProfileManagerPreloadingTest(/*delay_ms=*/"0",
                                         /*min_required_ram_mb=*/"4096") {}
  ~GlicProfileManagerLowMemoryPreloadingTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         GlicProfileManagerLowMemoryPreloadingTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(GlicProfileManagerLowMemoryPreloadingTest,
                       ShouldPreloadForProfile_LowMemoryDevice) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();

  // Set the physical memory override to 2GB (2048MB), which is less than 4GB.
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiBU(2));

  EXPECT_EQ(WaitForShouldPreload(),
            GlicPrewarmingChecksResult::kDeviceLowMemory);
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerLowMemoryPreloadingTest,
                       ShouldPreloadForProfile_SufficientMemory) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();

  // Set the physical memory override to 8GB (8192MB), which is greater than
  // 4GB.
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiBU(8));

  EXPECT_EQ(WaitForShouldPreload(), GlicPrewarmingChecksResult::kSuccess);
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_Cellular) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  SetConnectionType(net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  EXPECT_EQ(WaitForShouldPreload(),
            GlicPrewarmingChecksResult::kCellularConnection);
}

// See *Deferred* below. Checks that we don't defer preloading when there's no
// delay.
IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_DoNotDefer) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->GetProfile());
  service->TryPreload();
  // Since we have no delay, running until idle should mean that we do warm
  // (provided warming is enabled).
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsWarmed());
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicProfileManagerPreloadingTest,
                         ::testing::Bool());

class GlicProfileManagerDeferredPreloadingTest
    : public GlicProfileManagerPreloadingTest {
 public:
  // This sets the delay to 500 ms.
  GlicProfileManagerDeferredPreloadingTest()
      : GlicProfileManagerPreloadingTest(/*delay_ms=*/"500") {}
  ~GlicProfileManagerDeferredPreloadingTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This is really a keyed service test, but it is convenient to locate it here
// for now. It just checks that if we have a preload delay, that we won't
// preload immediately.
IN_PROC_BROWSER_TEST_P(GlicProfileManagerDeferredPreloadingTest,
                       ShouldPreloadForProfile_Defer) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->GetProfile());
  service->TryPreload();
  // Since we shouldn't preload until after the delay, we shouldn't be warmed
  // after running until idle.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsWarmed());
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerDeferredPreloadingTest,
                       ShouldPreloadForProfile_DeferWithProfileDeletion) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->GetProfile());
  base::RunLoop run_loop;
  service->AddPreloadCallback(run_loop.QuitClosure());
  service->TryPreload();
  service->reset_profile_for_test();
  run_loop.Run();
  EXPECT_FALSE(IsWarmed());
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicProfileManagerDeferredPreloadingTest,
                         ::testing::Bool());

}  // namespace

class GlicProfileManagerDidSelectProfileTest
    : public GlicProfileManagerBrowserTest {
 public:
  GlicProfileManagerDidSelectProfileTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicMultiInstance, mojom::features::kGlicMultiTab,
         features::kGlicMultitabUnderlines},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicProfileManagerDidSelectProfileTest,
                       DidSelectProfile_NoConsent) {
  // Create a profile that is eligible but has not consented.
  Profile* profile =
      CreateNewProfile(/*signin_and_allow_glic=*/false);
  SigninWithPrimaryAccount(profile);
  SetGlicCapability(profile, true);
  glic::GlicKeyedService::Get(profile)->enabling().SetCompletedFre(
      glic::prefs::FreStatus::kNotStarted);
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));
  ASSERT_FALSE(GlicEnabling::HasConsentedForProfile(profile));

  auto* service = GetMockGlicKeyedService(profile);

  EXPECT_CALL(*service,
              ShowUI(nullptr, mojom::InvocationSource::kProfilePicker));

  GlicProfileManager::GetInstance()->DidSelectProfile(profile);
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerDidSelectProfileTest,
                       DidSelectProfile_Consented) {
  // Create a profile that is eligible and has consented.
  Profile* profile =
      CreateNewProfile(/*signin_and_allow_glic=*/true);
  glic::GlicKeyedService::Get(profile)->enabling().SetCompletedFre(
      glic::prefs::FreStatus::kCompleted);
  ASSERT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile));

  auto* service = GetMockGlicKeyedService(profile);

  EXPECT_CALL(*service, ShowUI(testing::IsNull(),
                               mojom::InvocationSource::kProfilePicker));

  GlicProfileManager::GetInstance()->DidSelectProfile(profile);
}

}  // namespace glic
