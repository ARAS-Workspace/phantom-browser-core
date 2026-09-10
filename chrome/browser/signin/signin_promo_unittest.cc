// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/extensions/sync/extension_sync_util.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_test_helper.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_bookmarks/switches.h"
#include "content/public/test/browser_task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "extensions/common/extension_builder.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

TEST(SigninPromoTest, TestPromoURL) {
  GURL::Replacements replace_query;
  replace_query.SetQueryStr("access_point=0&reason=0&auto_close=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedPromoURL(signin_metrics::AccessPoint::kStartPage,
                          signin_metrics::Reason::kSigninPrimaryAccount, true));
  replace_query.SetQueryStr("access_point=15&reason=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedPromoURL(signin_metrics::AccessPoint::kFullscreenSigninPromo,
                          signin_metrics::Reason::kAddSecondaryAccount, false));
}

TEST(SigninPromoTest, TestReauthURL) {
  GURL::Replacements replace_query;
  replace_query.SetQueryStr(
      "access_point=0&reason=6&auto_close=1"
      "&email=example%40domain.com&validateEmail=1"
      "&readOnlyEmail=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedReauthURLWithEmail(signin_metrics::AccessPoint::kStartPage,
                                    signin_metrics::Reason::kFetchLstOnly,
                                    "example@domain.com"));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// This test can be deleted once kReplaceSyncPromosWithSignInPromos is launched.
// The behavior with the feature enabled is tested in
// SigninURLForDiceWithHistorySyncOptin.
class SigninPromoUrlTest : public testing::Test {
 public:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
};

TEST_F(SigninPromoUrlTest, SigninURLForDice) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          syncer::kReplaceSyncPromosWithSignInPromos,
          syncer::kReplaceSyncPromosWithSigninPromosNewSignin});

  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "color_scheme=dark&flow=promo&theme=mn",
      GetChromeSyncURLForDice(
          {.request_dark_scheme = true, .flow = Flow::PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "email_hint=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F&"
      "theme=mn",
      GetChromeSyncURLForDice(
          {"email@gmail.com", GURL("https://continue_url/")}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/"
      "sync?ssp=1&flow=embedded_promo&theme=mn",
      GetChromeSyncURLForDice({.flow = Flow::EMBEDDED_PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/AddSession?"
      "Email=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F",
      GetAddAccountURLForDice("email@gmail.com",
                              GURL("https://continue_url/")));
}

TEST_F(SigninPromoUrlTest, SigninURLForDiceWithHistorySyncOptin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos},
      /*disabled_features=*/{});
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "color_scheme=dark&flow=promo&theme=mn",
      GetChromeSyncURLForDice(
          {.request_dark_scheme = true, .flow = Flow::PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/"
      "sync?ssp=1&email_hint=email%40gmail.com&continue=https%3A%2F%2Fcontinue_"
      "url%2F&flow=history_opt_in&theme=mn",
      GetChromeSyncURLForDice(
          {"email@gmail.com", GURL("https://continue_url/")}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/"
      "sync?ssp=1&flow=embedded_promo&theme=mn",
      GetChromeSyncURLForDice({.flow = Flow::EMBEDDED_PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/AddSession?"
      "Email=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F",
      GetAddAccountURLForDice("email@gmail.com",
                              GURL("https://continue_url/")));
}

TEST_F(SigninPromoUrlTest, SigninURLForDiceMagiChromeExperiments) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{switches::kMagiChromeSignInExperimentsBatch1,
                             {{"magichrome_fre_exp_branch", "test_branch"}}},
                            {syncer::kReplaceSyncPromosWithSignInPromos, {}}},
      /*disabled_features=*/{});

  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "flow=history_opt_in&theme=mn&magichrome_fre_exp_branch=test_branch",
      GetChromeSyncURLForDice({}));
}

TEST(SigninPromoTest,
     IsHybridTransportSupportedForQrCodeSignin_LeNotSupported) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto bluetooth_override_values =
      device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  bluetooth_override_values->SetLESupported(false);

  base::test::TestFuture<bool> future;
  IsHybridTransportSupportedForQrCodeSignin(future.GetCallback());
  EXPECT_FALSE(future.Get());
  task_environment.RunUntilIdle();
}

TEST(SigninPromoTest,
     IsHybridTransportSupportedForQrCodeSignin_AdapterNotPresent) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto bluetooth_override_values =
      device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  bluetooth_override_values->SetLESupported(true);
  auto mock_adapter =
      base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
  ON_CALL(*mock_adapter, IsPresent()).WillByDefault(testing::Return(false));
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);

  base::test::TestFuture<bool> future;
  IsHybridTransportSupportedForQrCodeSignin(future.GetCallback());
  EXPECT_FALSE(future.Get());
  task_environment.RunUntilIdle();
}

TEST(SigninPromoTest,
     IsHybridTransportSupportedForQrCodeSignin_AdapterPresentAndPoweredOn) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto bluetooth_override_values =
      device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  bluetooth_override_values->SetLESupported(true);
  auto mock_adapter =
      base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
  ON_CALL(*mock_adapter, IsPresent()).WillByDefault(testing::Return(true));
  ON_CALL(*mock_adapter, GetOsPermissionStatus())
      .WillByDefault(testing::Return(
          device::BluetoothAdapter::PermissionStatus::kAllowed));
  ON_CALL(*mock_adapter, IsPowered()).WillByDefault(testing::Return(true));
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);

  base::test::TestFuture<bool> future;
  IsHybridTransportSupportedForQrCodeSignin(future.GetCallback());
  EXPECT_TRUE(future.Get());
  task_environment.RunUntilIdle();
}

TEST(SigninPromoTest,
     IsHybridTransportSupportedForQrCodeSignin_AdapterPoweredOff) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto bluetooth_override_values =
      device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  bluetooth_override_values->SetLESupported(true);
  auto mock_adapter =
      base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
  ON_CALL(*mock_adapter, IsPresent()).WillByDefault(testing::Return(true));
  ON_CALL(*mock_adapter, GetOsPermissionStatus())
      .WillByDefault(testing::Return(
          device::BluetoothAdapter::PermissionStatus::kAllowed));
  ON_CALL(*mock_adapter, IsPowered()).WillByDefault(testing::Return(false));
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);

  base::test::TestFuture<bool> future;
  IsHybridTransportSupportedForQrCodeSignin(future.GetCallback());
  EXPECT_FALSE(future.Get());
  task_environment.RunUntilIdle();
}

TEST(SigninPromoTest,
     IsHybridTransportSupportedForQrCodeSignin_PermissionDenied) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto bluetooth_override_values =
      device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  bluetooth_override_values->SetLESupported(true);
  auto mock_adapter =
      base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
  ON_CALL(*mock_adapter, IsPresent()).WillByDefault(testing::Return(true));
  ON_CALL(*mock_adapter, GetOsPermissionStatus())
      .WillByDefault(
          testing::Return(device::BluetoothAdapter::PermissionStatus::kDenied));
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);

  base::test::TestFuture<bool> future;
  IsHybridTransportSupportedForQrCodeSignin(future.GetCallback());
  EXPECT_FALSE(future.Get());
  task_environment.RunUntilIdle();
}

TEST(SigninPromoTest,
     IsHybridTransportSupportedForQrCodeSignin_PermissionUndetermined) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto bluetooth_override_values =
      device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  bluetooth_override_values->SetLESupported(true);
  auto mock_adapter =
      base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
  ON_CALL(*mock_adapter, IsPresent()).WillByDefault(testing::Return(true));
  ON_CALL(*mock_adapter, GetOsPermissionStatus())
      .WillByDefault(testing::Return(
          device::BluetoothAdapter::PermissionStatus::kUndetermined));
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);

  base::test::TestFuture<bool> future;
  IsHybridTransportSupportedForQrCodeSignin(future.GetCallback());
  EXPECT_FALSE(future.Get());
  task_environment.RunUntilIdle();
}

TEST(SigninPromoTest, IsSignInPromo_AutofillTypes) {
  EXPECT_TRUE(IsSignInPromo(signin_metrics::AccessPoint::kPasswordBubble));
  EXPECT_TRUE(IsSignInPromo(signin_metrics::AccessPoint::kAddressBubble));
}

TEST(SigninPromoTest, IsSignInPromo_SendTabToSelf) {
  EXPECT_TRUE(IsSignInPromo(signin_metrics::AccessPoint::kSendTabToSelfPromo));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// ChromeOS currently does not show any sign in promos.

// Extensions explicit signin is not enabled in ChromeOS.
TEST(SigninPromoTest, IsSignInPromo_ExtensionsWithExplicitSignin) {
  EXPECT_TRUE(
      IsSignInPromo(signin_metrics::AccessPoint::kExtensionInstallBubble));
}

TEST(SigninPromoTest, GetSignInPromoTypeFromAccessPoint) {
  EXPECT_EQ(SignInPromoType::kPassword,
            GetSignInPromoTypeFromAccessPoint(
                signin_metrics::AccessPoint::kPasswordBubble));
  EXPECT_EQ(SignInPromoType::kAddress,
            GetSignInPromoTypeFromAccessPoint(
                signin_metrics::AccessPoint::kAddressBubble));
  EXPECT_EQ(SignInPromoType::kBookmark,
            GetSignInPromoTypeFromAccessPoint(
                signin_metrics::AccessPoint::kBookmarkBubble));
  EXPECT_EQ(SignInPromoType::kExtension,
            GetSignInPromoTypeFromAccessPoint(
                signin_metrics::AccessPoint::kExtensionInstallBubble));
  EXPECT_EQ(SignInPromoType::kSendTabToSelf,
            GetSignInPromoTypeFromAccessPoint(
                signin_metrics::AccessPoint::kSendTabToSelfPromo));
}

class ShowPromoTest : public testing::Test {
 public:
  ShowPromoTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context) {
          return static_cast<std::unique_ptr<KeyedService>>(
              std::make_unique<syncer::MockSyncService>());
        }));
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(profile_builder);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
  }

  void SetUp() override {
    ON_CALL(*sync_service(), GetDataTypesForTransportOnlyMode())
        .WillByDefault(testing::Return(syncer::DataTypeSet::All()));
  }

  syncer::MockSyncService* sync_service() {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  IdentityManager* identity_manager() {
    return identity_test_env_adaptor_->identity_test_env()->identity_manager();
  }

  TestingProfile* profile() { return profile_.get(); }

  const extensions::Extension* CreateExtension(
      extensions::mojom::ManifestLocation location =
          extensions::mojom::ManifestLocation::kInternal) {
    extension_ = extensions::ExtensionBuilder()
                     .SetManifest(base::DictValue()
                                      .Set("name", "test")
                                      .Set("manifest_version", 2)
                                      .Set("version", "1.0.0"))
                     .SetLocation(location)
                     .Build();

    return extension_.get();
  }

 protected:
  void DisableSync() {
    ON_CALL(*sync_service(), GetDisableReasons())
        .WillByDefault(testing::Return(syncer::SyncService::DisableReasonSet(
            {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY})));
  }

  autofill::AutofillProfile CreateAddress(
      const std::string& country_code = "US") {
    return autofill::test::StandardProfile(
        autofill::AddressCountryCode(country_code));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  scoped_refptr<const extensions::Extension> extension_;
};

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ShowPromoTest, ShouldShowSigninPromoSyncDisabled) {
  DisableSync();
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowExtensionSignInPromo(*profile(), *CreateExtension()));
}

TEST_F(ShowPromoTest, ShouldShowSigninPromoSyncEnabled) {
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowExtensionSignInPromo(*profile(), *CreateExtension()));
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ShowSigninPromoTestWithFeatureFlags : public ShowPromoTest {
 public:
  void SetUp() override {
    ShowPromoTest::SetUp();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {syncer::kReplaceSyncPromosWithSignInPromos,
         syncer::kUnoPhase2FollowUp},
        /*disabled_features=*/{});
  }

  GaiaId gaia_id() {
    return identity_manager()
        ->GetPrimaryAccountInfo(ConsentLevel::kSignin)
        .gaia;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ShowSigninPromoTestWithFeatureFlags, ShowPromoWithNoAccount) {
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, ShowPromoWithWebSignedInAccount) {
  MakeAccountAvailable(identity_manager(), "test@email.com");
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, ShowPromoWithSignInPendingAccount) {
  AccountInfo info = MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", ConsentLevel::kSignin);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithAlreadySignedInAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithAlreadySyncingAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSync);
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, DoNotShowPromoWithNoSyncService) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      SyncServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context) {
        return static_cast<std::unique_ptr<KeyedService>>(nullptr);
      }));

  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(profile_builder);

  ASSERT_EQ(nullptr, SyncServiceFactory::GetForProfile(profile.get()));
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithOffTheRecordProfile) {
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(
      *profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithLocalSyncEnabled) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetBoolean(syncer::prefs::kEnableLocalSyncBackend,
                                    true);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, DoNotShowPromoWithoutSyncAllowed) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  DisableSync();

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithTypeManagedByPolicy) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  ON_CALL(*sync_service()->GetMockUserSettings(),
          IsTypeManagedByPolicy(syncer::UserSelectableType::kPasswords))
      .WillByDefault(testing::Return(true));

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithoutTransportOnlyDataType) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  ON_CALL(*sync_service(), GetDataTypesForTransportOnlyMode())
      .WillByDefault(testing::Return(syncer::DataTypeSet()));

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, ShowExtensionsPromoWithNoAccount) {
  EXPECT_TRUE(ShouldShowExtensionSignInPromo(*profile(), *CreateExtension()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       ShowExtensionsPromoWithSignInPendingAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  EXPECT_TRUE(ShouldShowExtensionSignInPromo(*profile(), *CreateExtension()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowExtensionPromoWithUnpackedExtension) {
  const extensions::Extension* unpacked_extension =
      CreateExtension(extensions::mojom::ManifestLocation::kUnpacked);

  // Unpacked extensions cannot be synced so the sign in promo is not shown.
  ASSERT_TRUE(unpacked_extension);
  ASSERT_FALSE(
      extensions::sync_util::ShouldSync(profile(), unpacked_extension));
  EXPECT_FALSE(ShouldShowExtensionSignInPromo(*profile(), *unpacked_extension));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPasswordPromoAfterFiveTimesShown) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoShownCountPerProfile, 5);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowAddressPromoAfterFiveTimesShown) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAddressSignInPromoShownCountPerProfile, 5);

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowBookmarkPromoAfterFiveTimesShown) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kBookmarkSignInPromoShownCountPerProfile, 5);

  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoAfterTwoTimesDismissed) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 2);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       ShowPromoAfterTwoTimesDismissedByDifferentAccounts) {
  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 1);
  SigninPrefs prefs(*profile()->GetPrefs());
  prefs.IncrementAutofillSigninPromoDismissCount(GaiaId("gaia_id"));

  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowAddressIfProfileMigrationBlocked) {
  autofill::AutofillProfile address = autofill::test::StandardProfile();
  autofill::PersonalDataManagerFactory::GetForBrowserContext(profile())
      ->address_data_manager()
      .AddMaxStrikesToBlockProfileMigration(address.guid());
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), address));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       ShowBookmarkPromoInSignInPendingState) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Promo is showing in sign in pending with account storage enabled.
  ON_CALL(*sync_service()->GetMockUserSettings(), GetSelectedTypes())
      .WillByDefault(testing::Return(syncer::UserSelectableTypeSet(
          {syncer::UserSelectableType::kBookmarks})));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  // Promo is showing in sign in pending with account storage disabled.
  ON_CALL(*sync_service()->GetMockUserSettings(), GetSelectedTypes())
      .WillByDefault(testing::Return(syncer::UserSelectableTypeSet()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  // Promo is showing when not in sign in pending with account storage disabled.
  ClearPrimaryAccount(identity_manager());
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       RecordSignInPromoShownWithoutAccount) {
  // Add an account without cookies. The per-profile pref will be recorded.
  AccountInfo account =
      MakeAccountAvailable(identity_manager(), "test@email.com");

  RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                         profile());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                         profile());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                         profile());

  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   prefs::kPasswordSignInPromoShownCountPerProfile));
  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   prefs::kAddressSignInPromoShownCountPerProfile));
  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   prefs::kBookmarkSignInPromoShownCountPerProfile));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetBookmarkSigninPromoImpressionCount(account.gaia));

  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       RecordSignInPromoShownWithoutAccount_PromoShouldShowForDifferentType) {
  // Add an account without cookies. The per-profile pref will be recorded.
  AccountInfo account =
      MakeAccountAvailable(identity_manager(), "test@email.com");

  // Show the password promo five times. This does not influence whether the
  // address promo should be shown.
  for (int i = 0; i < 5; i++) {
    RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                           profile());
  }

  EXPECT_EQ(5, profile()->GetPrefs()->GetInteger(
                   prefs::kPasswordSignInPromoShownCountPerProfile));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, RecordSignInPromoShownWithAccount) {
  // Test setup for adding an account with cookies.
  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  ChromeSigninClientFactory::GetInstance(),
                  base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                      &url_loader_factory)}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  // Add an account with cookies, which will record the per-account prefs.
  AccountInfo account = identity_test_env->MakeAccountAvailable(
      identity_test_env->CreateAccountAvailabilityOptionsBuilder()
          .WithCookie(true)
          .Build("test@email.com"));

  RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                         profile.get());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                         profile.get());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                         profile.get());

  EXPECT_EQ(0, profile.get()->GetPrefs()->GetInteger(
                   prefs::kPasswordSignInPromoShownCountPerProfile));
  EXPECT_EQ(0, profile.get()->GetPrefs()->GetInteger(
                   prefs::kAddressSignInPromoShownCountPerProfile));
  EXPECT_EQ(0, profile.get()->GetPrefs()->GetInteger(
                   prefs::kBookmarkSignInPromoShownCountPerProfile));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetBookmarkSigninPromoImpressionCount(account.gaia));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       RecordSignInPromoShownWithAccount_PromoShouldShowForDifferentType) {
  // Test setup for adding an account with cookies.
  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                   ChromeSigninClientFactory::GetInstance(),
                   base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                       &url_loader_factory)},
               TestingProfile::TestingFactory{
                   SyncServiceFactory::GetInstance(),
                   base::BindRepeating([](content::BrowserContext* context) {
                     return static_cast<std::unique_ptr<KeyedService>>(
                         std::make_unique<syncer::MockSyncService>());
                   })}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  ON_CALL(*static_cast<syncer::MockSyncService*>(
              SyncServiceFactory::GetForProfile(profile.get())),
          GetDataTypesForTransportOnlyMode())
      .WillByDefault(testing::Return(syncer::DataTypeSet::All()));

  // Add an account with cookies, which will record the per-account prefs.
  AccountInfo account = identity_test_env->MakeAccountAvailable(
      identity_test_env->CreateAccountAvailabilityOptionsBuilder()
          .WithCookie(true)
          .Build("test@email.com"));

  // Show the address promo five times. This does not influence whether the
  // password promo should be shown.
  for (int i = 0; i < 5; i++) {
    RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                           profile.get());
  }

  EXPECT_EQ(0, profile->GetPrefs()->GetInteger(
                   prefs::kAddressSignInPromoShownCountPerProfile));
  EXPECT_EQ(5, SigninPrefs(*profile.get()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile.get(), CreateAddress()));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile.get()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile.get()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       RecordSignInPromoShownWithAccount_BookmarkPromoNotAlwaysShown) {
  // Test setup for adding an account with cookies.
  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                   ChromeSigninClientFactory::GetInstance(),
                   base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                       &url_loader_factory)},
               TestingProfile::TestingFactory{
                   SyncServiceFactory::GetInstance(),
                   base::BindRepeating([](content::BrowserContext* context) {
                     return static_cast<std::unique_ptr<KeyedService>>(
                         std::make_unique<syncer::MockSyncService>());
                   })}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  ON_CALL(*static_cast<syncer::MockSyncService*>(
              SyncServiceFactory::GetForProfile(profile.get())),
          GetDataTypesForTransportOnlyMode())
      .WillByDefault(testing::Return(syncer::DataTypeSet::All()));

  // Add an account with cookies, which will record the per-account prefs.
  identity_test_env->MakeAccountAvailable(
      identity_test_env->CreateAccountAvailabilityOptionsBuilder()
          .WithCookie(true)
          .Build("test@email.com"));
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile.get()));

  // Show the bookmark promo five times. After this, the bookmark promo will not
  // be shown again.
  for (int i = 0; i < 5; i++) {
    RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                           profile.get());
  }

  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile.get()));
}
class ShowSigninPromoTestWithoutPhase2FollowUp
    : public ShowSigninPromoTestWithFeatureFlags {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {syncer::kReplaceSyncPromosWithSignInPromos},
        /*disabled_features=*/{syncer::kUnoPhase2FollowUp});
    ON_CALL(*sync_service(), GetDataTypesForTransportOnlyMode())
        .WillByDefault(testing::Return(syncer::DataTypeSet::All()));
  }
};

TEST_F(ShowSigninPromoTestWithoutPhase2FollowUp,
       RecordSignInPromoShownWithoutAccount_BookmarkPromoAlwaysShown) {
  // Add an account without cookies. The per-profile pref will be recorded.
  MakeAccountAvailable(identity_manager(), "test@email.com");
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  // Show the bookmark promo five times. This does not influence whether it is
  // shown again or not.
  for (int i = 0; i < 5; i++) {
    RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                           profile());
  }

  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithoutPhase2FollowUp,
       RecordSignInPromoShownWithAccount_BookmarkPromoAlwaysShown) {
  // Test setup for adding an account with cookies.
  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                   ChromeSigninClientFactory::GetInstance(),
                   base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                       &url_loader_factory)},
               TestingProfile::TestingFactory{
                   SyncServiceFactory::GetInstance(),
                   base::BindRepeating([](content::BrowserContext* context) {
                     return static_cast<std::unique_ptr<KeyedService>>(
                         std::make_unique<syncer::MockSyncService>());
                   })}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  ON_CALL(*static_cast<syncer::MockSyncService*>(
              SyncServiceFactory::GetForProfile(profile.get())),
          GetDataTypesForTransportOnlyMode())
      .WillByDefault(testing::Return(syncer::DataTypeSet::All()));

  // Add an account with cookies, which will record the per-account prefs.
  identity_test_env->MakeAccountAvailable(
      identity_test_env->CreateAccountAvailabilityOptionsBuilder()
          .WithCookie(true)
          .Build("test@email.com"));
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile.get()));

  // Show the bookmark promo five times. This does not influence whether it is
  // shown again or not.
  for (int i = 0; i < 5; i++) {
    RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                           profile.get());
  }

  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile.get()));
}

class ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment
    : public ShowSigninPromoTestWithFeatureFlags {
 public:
  void SetUp() override {
    ShowSigninPromoTestWithFeatureFlags::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        switches::kSigninPromoLimitsExperiment);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowAddressPromoAfterMaxTimesShown) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAddressSignInPromoShownCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoShownThreshold.Get());

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowPasswordPromoAfterMaxTimesShown) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoShownCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoShownThreshold.Get());

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowBookmarkPromoAfterMaxTimesShown) {
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment, 20);

  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       RecordSignInPromoShownWithoutAccount) {
  // Add an account without cookies. The per-profile pref will be recorded.
  AccountInfo account =
      MakeAccountAvailable(identity_manager(), "test@email.com");

  RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                         profile());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                         profile());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                         profile());

  EXPECT_EQ(
      1,
      profile()->GetPrefs()->GetInteger(
          prefs::kPasswordSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(
      1,
      profile()->GetPrefs()->GetInteger(
          prefs::kAddressSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(
      1,
      profile()->GetPrefs()->GetInteger(
          prefs::kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetBookmarkSigninPromoImpressionCount(account.gaia));

  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       RecordSignInPromoShownWithAccount) {
  // Test setup for adding an account with cookies.
  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  ChromeSigninClientFactory::GetInstance(),
                  base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                      &url_loader_factory)}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  // Add an account with cookies, which will record the per-account prefs.
  AccountInfo account = identity_test_env->MakeAccountAvailable(
      identity_test_env->CreateAccountAvailabilityOptionsBuilder()
          .WithCookie(true)
          .Build("test@email.com"));

  RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                         profile.get());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                         profile.get());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                         profile.get());

  EXPECT_EQ(
      0,
      profile.get()->GetPrefs()->GetInteger(
          prefs::kPasswordSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(
      0,
      profile.get()->GetPrefs()->GetInteger(
          prefs::kAddressSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(
      0,
      profile.get()->GetPrefs()->GetInteger(
          prefs::kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetBookmarkSigninPromoImpressionCount(account.gaia));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       SkipCheckForNonExperimentNumberOfTimesShownForPasswordPromo) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoShownCountPerProfile, INT_MAX);

  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       SkipCheckForNonExperimentNumberOfTimesShownForAddressPromo) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAddressSignInPromoShownCountPerProfile, INT_MAX);

  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       SkipCheckForNonExperimentNumberOfTimesShownForBookmarkPromo) {
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kBookmarkSignInPromoShownCountPerProfile, INT_MAX);

  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowPasswordPromoAfterMaxTimesDismissed) {
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoDismissCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoDismissedThreshold.Get());

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowAddressPromoAfterMaxTimesDismissed) {
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAddressSignInPromoDismissCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoDismissedThreshold.Get());

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowBookmarkPromoAfterMaxTimesDismissed) {
  base::test::ScopedFeatureList scoped_feature_list{syncer::kUnoPhase2FollowUp};

  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kBookmarkSignInPromoDismissCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoDismissedThreshold.Get());

  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
