// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/signin_intercept_first_run_experience_dialog.h"

#include "base/containers/enum_set.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace {

// Sync might use email address as an heuristic to determine whether an account
// might be managed.
const char kConsumerEmail[] = "test@example.com";
const char kEnterpriseEmail[] = "test@managed.com";

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

// Browser tests for SigninInterceptFirstRunExperienceDialog.
using TestBase = InteractiveFeaturePromoTestMixin<SigninBrowserTestBase>;

class SigninInterceptFirstRunExperienceDialogBrowserTestBase : public TestBase {
 public:
  using DialogEvent = SigninInterceptFirstRunExperienceDialog::DialogEvent;
  using DialogEventSet =
      base::EnumSet<DialogEvent, DialogEvent::kStart, DialogEvent::kMaxValue>;

  SigninInterceptFirstRunExperienceDialogBrowserTestBase()
      : TestBase(UseDefaultTrackerAllowingPromos(
                     {feature_engagement::kIPHProfileSwitchFeature},
                     TrackerInitializationMode::kDoNotWait),
                 ClockMode::kUseTestClock,
                 InitialSessionState::kOutsideGracePeriod,
                 /*use_main_profile=*/true),
        scoped_iph_delay_(
            AvatarToolbarButton::SetScopedIPHMinDelayAfterCreationForTesting(
                base::Seconds(0))) {}

  ~SigninInterceptFirstRunExperienceDialogBrowserTestBase() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    TestBase::SetUpInProcessBrowserTestFixture();

    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    TestBase::OnWillCreateBrowserContextServices(context);

    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
  }

  void SetUpOnMainThread() override {
    TestBase::SetUpOnMainThread();
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
    SetUpNavigationObservers();
  }

  void SetUpNavigationObservers() {
    sync_confirmation_observer_ =
        std::make_unique<content::TestNavigationObserver>(kSyncConfirmationUrl);
    profile_customization_observer_ =
        std::make_unique<content::TestNavigationObserver>(
            kProfileCustomizationUrl);
  }

  // Returns true if the profile switch IPH has been shown.
  bool ProfileSwitchPromoHasBeenShown() {
    return RunTestSequence(
        WaitForPromo(feature_engagement::kIPHProfileSwitchFeature));
  }

  void UpdateChromePolicy(const policy::PolicyMap& policy) {
    policy_provider_.UpdateChromePolicy(policy);
  }

  void SignIn(bool update_extended_account_info = true,
              bool disable_history_sync = true) {
    account_id_ = identity_test_env()
                      ->MakePrimaryAccountAvailable(
                          GetEmail(), signin::ConsentLevel::kSignin)
                      .account_id;
    EXPECT_EQ(
        identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
        account_id());
    // In the testing framework be default all data types are enabled for syncing, unless
    // we explicitly disabled them.
    ASSERT_TRUE(sync_service()->GetUserSettings()->IsSyncEverythingEnabled());
    // Disable history, tabs and tab groups syncing do that the history sync
    // optin screen is offered, if all the necessary conditions are met.
    if (disable_history_sync) {
      DisableHistorySync();
    }
    if (update_extended_account_info) {
      UpdateExtendedAccountInfo();
    }
  }

  void ExpectAndApproveSyncConfirmationDialog() {
    sync_confirmation_observer_->Wait();
    EXPECT_EQ(dialog()
                  ->GetModalDialogWebContentsForTesting()
                  ->GetLastCommittedURL(),
              kSyncConfirmationUrl);

    SimulateSyncConfirmationUIClosing(
        LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
    ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);

    // The dialog still shows the sync confirmation while waiting for the
    // synced theme to be applied.
    EXPECT_TRUE(controller()->ShowsModalDialog());
    EXPECT_EQ(dialog()
                  ->GetModalDialogWebContentsForTesting()
                  ->GetLastCommittedURL(),
              kSyncConfirmationUrl);
  }

  void SimulateSyncConfirmationUIClosing(
      LoginUIService::SyncConfirmationUIClosedResult result) {
    LoginUIServiceFactory::GetForProfile(GetProfile())
        ->SyncConfirmationUIClosed(result);
  }

  void SimulateProfileCustomizationDoneButtonClicked() {
    dialog()->ProfileCustomizationCloseOnCompletion(
        ProfileCustomizationHandler::CustomizationResult::kDone);
  }

  void SimulateProfileCustomizationSkipButtonClicked() {
    dialog()->ProfileCustomizationCloseOnCompletion(
        ProfileCustomizationHandler::CustomizationResult::kSkip);
  }

  void ExpectRecordedEvents(DialogEventSet events) {
    std::vector<base::Bucket> expected_buckets;
    for (DialogEvent event : events) {
      expected_buckets.emplace_back(static_cast<int>(event), 1);
    }
    EXPECT_THAT(histogram_tester_.GetAllSamples("Signin.Intercept.FRE.Event"),
                ::testing::ContainerEq(expected_buckets));
  }

  void ExpectSigninHistogramsRecorded() {
    const auto access_point =
        signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience;
    histogram_tester_.ExpectUniqueSample("Signin.SigninStartedAccessPoint",
                                         access_point, 1);
    histogram_tester_.ExpectUniqueSample("Signin.SigninCompletedAccessPoint",
                                         access_point, 1);
    EXPECT_EQ(user_action_tester_.GetActionCount(
                  "Signin_Signin_FromSigninInterceptFirstRunExperience"),
              1);
  }

 protected:
  // `kSignin` consent level means that Sync should be disabled.
  void ExpectPrimaryAccountWithExactConsentLevel(
      signin::ConsentLevel consent_level) {
    EXPECT_EQ(
        identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
        account_id());
    EXPECT_EQ(consent_level,
              signin::GetPrimaryAccountConsentLevel(identity_manager()));
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetProfile()));
  }

  ThemeService* theme_service() {
    return ThemeServiceFactory::GetForProfile(GetProfile());
  }

  SigninViewController* controller() {
    return browser()->GetFeatures().signin_view_controller();
  }

  SigninInterceptFirstRunExperienceDialog* dialog() {
    return static_cast<SigninInterceptFirstRunExperienceDialog*>(
        controller()->GetModalDialogForTesting());
  }

  CoreAccountId account_id() { return account_id_; }

  void DisableHistorySync() {
    sync_service()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, false);
    sync_service()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kTabs, false);
    sync_service()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kSavedTabGroups, false);
  }

  virtual std::string GetEmail() = 0;

  void UpdateExtendedAccountInfo() {
    AccountInfo account_info =
        identity_manager()->FindExtendedAccountInfoByAccountId(account_id());
    std::string hosted_domain =
        GetEmail() == kEnterpriseEmail ? "managed.com" : "";
    account_info = AccountInfo::Builder(account_info)
                       .SetFullName("fullname")
                       .SetGivenName("givenname")
                       .SetLocale("en")
                       .SetAvatarUrl("https://example.com")
                       .SetHostedDomain(hosted_domain)
                       .Build();
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
  }

 protected:
  const GURL kSyncConfirmationUrl = AppendSyncConfirmationQueryParams(
      GURL("chrome://sync-confirmation"),
      SyncConfirmationStyle::kSigninInterceptModal,
      /*is_sync_promo=*/true);
  const GURL kProfileCustomizationUrl = GURL("chrome://profile-customization");
  const GURL kSyncSettingsUrl = GURL("chrome://settings/syncSetup");

  std::unique_ptr<content::TestNavigationObserver> sync_confirmation_observer_;
  std::unique_ptr<content::TestNavigationObserver>
      profile_customization_observer_;

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  // Needed for profile switch IPH testing.
  base::AutoReset<base::TimeDelta> scoped_iph_delay_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

  CoreAccountId account_id_;
};

class SigninInterceptFirstRunExperienceDialogBrowserTest
    : public SigninInterceptFirstRunExperienceDialogBrowserTestBase,
      public ::testing::WithParamInterface<std::tuple<bool, std::string>> {
 public:
  SigninInterceptFirstRunExperienceDialogBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (std::get<0>(GetParam())) {
      enabled_features = {syncer::kReplaceSyncPromosWithSignInPromos,
                          syncer::kReplaceSyncPromosWithSigninPromosNewSignin,
                          syncer::kUnoPhase2FollowUp};
    } else {
      disabled_features = {syncer::kReplaceSyncPromosWithSignInPromos,
                           syncer::kReplaceSyncPromosWithSigninPromosNewSignin,
                           syncer::kUnoPhase2FollowUp};
    }
    scoped_features_.InitWithFeatures(enabled_features, disabled_features);
  }

  std::string GetEmail() override { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Shows and closes the fre dialog.
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       ShowAndCloseDialog) {
  SignIn();
  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /*is_forced_intercept=*/false);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  controller()->CloseModalSignin();
  EXPECT_FALSE(controller()->ShowsModalDialog());
}

// Goes through all steps of the fre dialog. The user enables sync or history
// sync (depending on which screen is offered).
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSync) {
  SignIn();
  sync_confirmation_observer_->StartWatchingNewWebContents();
  profile_customization_observer_->StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /*is_forced_intercept=*/false);

  ExpectAndApproveSyncConfirmationDialog();

  theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
      ThemeSyncableService::ThemeSyncState::kApplied);

  profile_customization_observer_->Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());

  ExpectRecordedEvents({DialogEvent::kStart,
                        DialogEvent::kShowSyncConfirmation,
                        DialogEvent::kSyncConfirmationClickConfirm,
                        DialogEvent::kShowProfileCustomization,
                        DialogEvent::kProfileCustomizationClickDone});
  // Note: the account was already signed in, so the histograms should not be
  // recorded. `TurnOnSyncHelper` records them anyway.
  ExpectSigninHistogramsRecorded();
}

// TODO(crbug.com/418143300): Add a test case for the history sync optin screen
// for managed users with a delayed execution of the management handling
// callback to ensure the history sync optin is eventually shown.

// Goes through all steps of the fre dialog and skips profile customization.
// The user enables sync or history sync (depending on which screen is offered).
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSyncSkipCustomization) {
  SignIn();
  sync_confirmation_observer_->StartWatchingNewWebContents();
  profile_customization_observer_->StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /*is_forced_intercept=*/false);
  EXPECT_TRUE(controller()->ShowsModalDialog());

  ExpectAndApproveSyncConfirmationDialog();

  theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
      ThemeSyncableService::ThemeSyncState::kApplied);

  profile_customization_observer_->Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationSkipButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());

  ExpectRecordedEvents({DialogEvent::kStart,
                        DialogEvent::kShowSyncConfirmation,
                        DialogEvent::kSyncConfirmationClickConfirm,
                        DialogEvent::kShowProfileCustomization,
                        DialogEvent::kProfileCustomizationClickSkip});
  // Note: the account was already signed in, so the histograms should not be
  // recorded. `TurnOnSyncHelper` records them anyway.
  ExpectSigninHistogramsRecorded();
  // TODO(crbug.com/40209493): test that the Skip button undoes the
  // changes in the theme color and the profile name.
}

// The user enables sync and has a synced extension theme. Tests that the dialog
// waits on the sync confirmation page until the extension theme is applied.
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSyncExtensionTheme) {
  SignIn();

  sync_confirmation_observer_->StartWatchingNewWebContents();
  profile_customization_observer_->StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /*is_forced_intercept=*/false);
  EXPECT_TRUE(controller()->ShowsModalDialog());

  ExpectAndApproveSyncConfirmationDialog();

  theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
      ThemeSyncableService::ThemeSyncState::kWaitingForExtensionInstallation);

  EXPECT_TRUE(controller()->ShowsModalDialog());
  // The dialog still shows the sync confirmation while waiting for the
  // extension theme to be downloaded and applied.
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  // Trigger a new theme being applied. Use an autogenerated theme instead of an
  // extension theme because it's easier to trigger and doesn't make any
  // difference for this test.
  theme_service()->BuildAutogeneratedThemeFromColor(SK_ColorGREEN);
  theme_service()
      ->GetThemeSyncableService()
      ->NotifyOnSyncStartedForTesting(  // why the former is not enough to
                                        // trigger the update?
          ThemeSyncableService::ThemeSyncState::kApplied);

  profile_customization_observer_->Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
}

// Tests that the profile customization is not shown when the user enables sync
// for an account with a custom passphrase.
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSyncCustomPassphrase) {
  SignIn();
  sync_confirmation_observer_->StartWatchingNewWebContents();
  profile_customization_observer_->StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /*is_forced_intercept=*/false);
  EXPECT_TRUE(controller()->ShowsModalDialog());

  // Note: In the Uno model, for this test case it does not
  // matter if we accept or reject this dialog.
  ExpectAndApproveSyncConfirmationDialog();
  sync_service()->SetPassphraseRequired();
  sync_service()->FireStateChanged();
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !controller()->ShowsModalDialog(); }));
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());

  ExpectRecordedEvents({DialogEvent::kStart,
                        DialogEvent::kShowSyncConfirmation,
                        DialogEvent::kSyncConfirmationClickConfirm});
  // Note: the account was already signed in, so the histograms should not be
  // recorded. `TurnOnSyncHelper` records them anyway.
  ExpectSigninHistogramsRecorded();
}

// Goes through all steps of the fre dialog.
// The user declines sync or history sync (depending on which screen is
// offered).
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       DeclineSync) {
  SignIn();
  sync_confirmation_observer_->StartWatchingNewWebContents();
  profile_customization_observer_->StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /*is_forced_intercept=*/false);
  EXPECT_TRUE(controller()->ShowsModalDialog());

  sync_confirmation_observer_->Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  SimulateSyncConfirmationUIClosing(LoginUIService::ABORT_SYNC);

  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  profile_customization_observer_->Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());

  ExpectRecordedEvents({DialogEvent::kStart,
                        DialogEvent::kShowSyncConfirmation,
                        DialogEvent::kSyncConfirmationClickCancel,
                        DialogEvent::kShowProfileCustomization,
                        DialogEvent::kProfileCustomizationClickDone});
  // Note: the account was already signed in, so the histograms should not be
  // recorded. `TurnOnSyncHelper` records them anyway.
  ExpectSigninHistogramsRecorded();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SigninInterceptFirstRunExperienceDialogBrowserTest,
    ::testing::Combine(::testing::Values(false),
                       ::testing::Values(kConsumerEmail, kEnterpriseEmail)),
    [](const auto& info) {
      return (std::get<0>(info.param) ? "InUnoModel" : "InDiceModel") +
             std::string((std::get<1>(info.param) == kConsumerEmail
                              ? "ForConsumer"
                              : "ForEnterprise"));
    });
