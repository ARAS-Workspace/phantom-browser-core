// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/barrier_closure.h"
#include "base/cfi_buildflags.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/scoped_multi_source_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/metrics/first_web_contents_profiler_base.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_test_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_active_state_manager/browser_active_state_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/profiles/profile_customization_util.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_handler.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_deletion_observer.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registrar.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/events/event_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

using signin::constants::kNoHostedDomainFound;
using testing::_;
using testing::Eq;
using testing::Pair;
using testing::UnorderedElementsAre;

// Sets up account capabilities so that the History Sync Opt-in is shown without
// any pending UI updates.
void MakeHistorySyncOptinUiAvailable(signin::IdentityManager& identity_manager,
                                     AccountInfo& account_info,
                                     bool eligible = true) {
  AccountCapabilitiesTestMutator(&account_info)
      .set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
          eligible);
  signin::UpdateAccountInfoForAccount(&identity_manager, account_info);
}

AccountInfo FillAccountInfo(
    const CoreAccountInfo& core_info,
    const std::string& given_name,
    const std::string& hosted_domain = kNoHostedDomainFound) {
  AccountInfo account_info = AccountInfo::Builder(core_info)
                                 .SetFullName("Test Full Name")
                                 .SetGivenName(given_name)
                                 .SetHostedDomain(hosted_domain)
                                 .SetLocale("en")
                                 .SetAvatarUrl("https://get-avatar.com/foo")
                                 .Build();
  bool is_managed = hosted_domain != kNoHostedDomainFound;
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_is_subject_to_enterprise_features(is_managed);
  mutator.set_is_subject_to_account_level_enterprise_policies(is_managed);
  return account_info;
}

class BrowserAddedWaiter : public BrowserCollectionObserver {
 public:
  enum class ReturnMode { kNew, kLatest };

  BrowserAddedWaiter(size_t total_count, ReturnMode return_mode)
      : total_count_(total_count), return_mode_(return_mode) {
    for (BrowserWindowInterface* b : GetAllBrowserWindowInterfaces()) {
      initial_browsers_.insert(b);
    }
    observation_.Observe(GlobalBrowserCollection::GetInstance());
  }

  BrowserAddedWaiter(const BrowserAddedWaiter&) = delete;
  BrowserAddedWaiter& operator=(const BrowserAddedWaiter&) = delete;

  ~BrowserAddedWaiter() override = default;

  BrowserWindowInterface* Wait() {
    if (browser_) {
      return browser_;
    }
    if (GlobalBrowserCollection::GetInstance()->GetSize() == total_count_) {
      switch (return_mode_) {
        case ReturnMode::kNew:
          for (BrowserWindowInterface* b : GetAllBrowserWindowInterfaces()) {
            if (initial_browsers_.find(b) == initial_browsers_.end()) {
              return b;
            }
          }
          break;
        case ReturnMode::kLatest:
          return GetLastActiveBrowserWindowInterfaceWithAnyProfile();
      }
    }
    run_loop_.Run();
    EXPECT_TRUE(browser_);
    return browser_;
  }

 private:
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    if (GlobalBrowserCollection::GetInstance()->GetSize() != total_count_) {
      return;
    }
    browser_ = browser;
    run_loop_.Quit();
  }

  const size_t total_count_;
  const ReturnMode return_mode_;
  raw_ptr<BrowserWindowInterface, AcrossTasksDanglingUntriaged> browser_ =
      nullptr;
  std::set<BrowserWindowInterface*> initial_browsers_;
  base::RunLoop run_loop_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation_{this};
};

class PageNonEmptyPaintObserver : public content::WebContentsObserver {
 public:
  explicit PageNonEmptyPaintObserver(const GURL& url,
                                     content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        barrier_closure_(base::BarrierClosure(2, run_loop_.QuitClosure())),
        url_(url) {}

  void Wait() {
    // Check if the right page has already been painted or loaded.
    if (web_contents()->GetLastCommittedURL() == url_) {
      if (web_contents()->CompletedFirstVisuallyNonEmptyPaint()) {
        DidFirstVisuallyNonEmptyPaint();
      }
      if (!web_contents()->IsLoading()) {
        DidStopLoading();
      }
    }

    run_loop_.Run();
  }

 private:
  // WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override {
    // Making sure that the same event does not trigger the barrier twice.
    if (did_paint_) {
      return;
    }

    did_paint_ = true;
    barrier_closure_.Run();
  }

  void DidStopLoading() override {
    ASSERT_EQ(web_contents()->GetLastCommittedURL(), url_);

    // Making sure that the same event does not trigger the barrier twice.
    if (did_load_) {
      return;
    }

    // It shouldn't technically be necessary to wait for load stop here, we do
    // this to be consistent with the other tests relying on `WaitForLoadStop()`
    did_load_ = true;
    barrier_closure_.Run();
  }

  base::RunLoop run_loop_;
  base::RepeatingClosure barrier_closure_;
  GURL url_;

  bool did_paint_ = false;
  bool did_load_ = false;
};

// Waits for a first non empty paint for `target` and expects that it will load
// the given `url`.
void WaitForFirstNonEmptyPaint(const GURL& url, content::WebContents* target) {
  ASSERT_NE(target, nullptr);

  PageNonEmptyPaintObserver observer(url, target);
  observer.Wait();
}

// Waits for the browser to load the given URL in the given WebContents. This
// observes `target` to wait for the specific `url` to be committed.
void WaitForBrowserUrl(const GURL& url, content::WebContents* target) {
  ASSERT_NE(target, nullptr);
  content::WaitForLoadStop(target);
  while (target->GetLastCommittedURL() != url) {
    content::TestNavigationObserver observer(target);
    observer.Wait();
    content::WaitForLoadStop(target);
  }
  EXPECT_EQ(target->GetLastCommittedURL(), url);
}

}  // namespace

class ProfilePickerViewBrowserTest : public ProfilePickerTestBase {};

// Regression test for crbug.com/40910391.
IN_PROC_BROWSER_TEST_F(ProfilePickerViewBrowserTest,
                       ShowScreen_DoesNotFinishForErrorOnInternalNavigation) {
  GURL bad_target_url{"chrome://unregistered-host"};
  base::test::TestFuture<void> navigation_finished_future;

  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL{"chrome://profile-picker"});
  view()->ShowScreenInPickerContents(bad_target_url,
                                     navigation_finished_future.GetCallback());

  WaitForLoadStop(bad_target_url, web_contents());
  EXPECT_FALSE(navigation_finished_future.IsReady());
}

// Regression test for crbug.com/40910391.
IN_PROC_BROWSER_TEST_F(ProfilePickerViewBrowserTest,
                       ShowScreen_FinishesForErrorOnStandardNavigation) {
  // URL intended to simulate an https navigation that fails because the host
  // can't be found. With an internet connection it would redirect to the
  // DNS_PROBE_FINISHED_NXDOMAIN error page. Simulate that in the picker flow
  // using the `--gaia-url` command line parameter.
  // During tests all external navigations fail anyway, but that's good enough
  // for what we're trying to verify.
  GURL bad_target_url{"https://url.madeup"};
  base::test::TestFuture<void> navigation_finished_future;

  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL{"chrome://profile-picker"});
  view()->ShowScreenInPickerContents(bad_target_url,
                                     navigation_finished_future.GetCallback());

  WaitForLoadStop(bad_target_url, web_contents());
  EXPECT_TRUE(navigation_finished_future.IsReady());
}

class ProfilePickerCreationFlowBrowserTest
    : public InteractiveFeaturePromoTestMixin<ProfilePickerTestBase> {
 public:
  ProfilePickerCreationFlowBrowserTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHProfileSwitchFeature,
             feature_engagement::kIPHSupervisedUserProfileSigninFeature})) {
#if BUILDFLAG(IS_MAC)
    // Ensure the platform is unmanaged
    platform_management_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForPlatform(),
            policy::EnterpriseManagementAuthority::NONE);
#endif
  }

  void SetUpInProcessBrowserTestFixture() override {
    InteractiveFeaturePromoTestMixin::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ProfilePickerCreationFlowBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&policy::FakeUserPolicySigninService::Build));

    // Clear the previous cookie responses (if any) before using it for a new
    // profile (as test_url_loader_factory() is shared across profiles).
    test_url_loader_factory()->ClearResponses();
    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     test_url_loader_factory()));
  }

  // Returns the initial page.
  GURL ShowPickerAndWait(bool start_on_management_page = false) {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        start_on_management_page
            ? ProfilePicker::EntryPoint::kProfileMenuManageProfiles
            : ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
    // Wait until webUI is fully initialized.
    const GURL kInitialPageUrl(start_on_management_page
                                   ? "chrome://profile-picker"
                                   : "chrome://profile-picker/new-profile");
    WaitForLoadStop(kInitialPageUrl);
    return kInitialPageUrl;
  }

  void SimulateEnableSyncDiceHeader(content::WebContents* contents,
                                    const CoreAccountInfo& account_info) {
    // Simulate the Dice "ENABLE_SYNC" header parameter.
    auto process_dice_header_delegate_impl =
        ProcessDiceHeaderDelegateImpl::Create(contents);
    process_dice_header_delegate_impl->CompleteChromeSignInAfterGaiaSignin(
        account_info);
  }

  AccountInfo FinishDiceSignIn(
      Profile* profile_being_created,
      const std::string& email,
      const std::string& given_name,
      const std::string& hosted_domain = kNoHostedDomainFound,
      bool is_supervised_profile = false,
      bool should_have_primary_account = true) {
    // Add an account - simulate a successful Gaia sign-in.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_being_created);
    CoreAccountInfo core_account_info = signin::MakeAccountAvailable(
        identity_manager,
        signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
            .WithAccessPoint(signin_metrics::AccessPoint::kUserManager)
            .Build(email));
    EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
        core_account_info.account_id));

    CHECK(profile_being_created);
    AccountInfo account_info =
        FillAccountInfo(core_account_info, given_name, hosted_domain);

    if (is_supervised_profile) {
      AccountCapabilitiesTestMutator mutator(&account_info);
      mutator.set_is_subject_to_parental_controls(true);
    }

    MakeHistorySyncOptinUiAvailable(*identity_manager, account_info,
                                    !is_supervised_profile);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);

    if (web_contents()) {
      SimulateEnableSyncDiceHeader(web_contents(), core_account_info);
      // If the sync header is received then the user is always signed in to the
      // browser.
      EXPECT_EQ(
          identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin),
          should_have_primary_account);
    }
    return account_info;
  }

  static bool HasPromoBeenShown(BrowserWindowInterface* browser,
                                const base::Feature& feature) {
    return BrowserUserEducationInterface::From(browser)->IsFeaturePromoActive(
               feature) ||
           BrowserUserEducationInterface::From(browser)->IsFeaturePromoQueued(
               feature);
  }

  // Returns true if the profile switch IPH has been shown.
  static bool ProfileSwitchPromoHasBeenShown(BrowserWindowInterface* browser) {
    return HasPromoBeenShown(browser,
                             feature_engagement::kIPHProfileSwitchFeature);
  }

  // Simulates a click on a profile card. The profile picker must be already
  // opened.
  void OpenProfileFromPicker(const base::FilePath& profile_path,
                             bool open_settings) {
    base::ListValue args;
    args.Append(base::FilePathToValue(profile_path));
    profile_picker_handler()->HandleLaunchSelectedProfile(open_settings, args);
  }

  // Simulates a click on "Browse as Guest".
  void OpenGuestFromPicker() {
    base::ListValue args;
    profile_picker_handler()->HandleLaunchGuestProfile(args);
  }

  // Creates a new profile without opening a browser.
  base::FilePath CreateNewProfileWithoutBrowser() {
    // Create a second profile.
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath path = profile_manager->GenerateNextProfileDirectoryPath();
    profiles::testing::CreateProfileSync(profile_manager, path);
    return path;
  }

  // Create a new Profile and destroy it immediately so that it is not loaded.
  base::FilePath CreateNewProfileAndUnload() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath new_profile_path =
        profile_manager->GenerateNextProfileDirectoryPath();

    Profile* new_profile = &profiles::testing::CreateProfileSync(
        profile_manager, new_profile_path);

    ProfileDestructionWaiter profile_destruction_waiter(new_profile);
    Browser* new_browser = CreateBrowser(new_profile);
    CloseBrowserSynchronously(new_browser);
    profile_destruction_waiter.Wait();

    return new_profile_path;
  }

  // Simulates a click on "Continue without an account" to create a local
  // profile and open the profile customization dialog.
  void CreateLocalProfile() {
    base::ListValue args;
    args.Append(base::Value());
    profile_picker_handler()->HandleContinueWithoutAccount(args);
  }

  // Simulates a click on "Done" on the Profile Customization to confirm the
  // creation of the local profile.
  void ConfirmLocalProfileCreation(content::WebContents* dialog_web_contents) {
    base::ListValue args;
    args.Append(base::Value(kLocalProfileName));
    dialog_web_contents->GetWebUI()
        ->GetController()
        ->GetAs<ProfileCustomizationUI>()
        ->GetProfileCustomizationHandlerForTesting()
        ->HandleDone(args);
  }

  // Simulates a click on "Delete profile" on the Profile Customization to
  // cancel the creation of the local profile.
  void DeleteLocalProfile(content::WebContents* dialog_web_contents) {
    dialog_web_contents->GetWebUI()
        ->GetController()
        ->GetAs<ProfileCustomizationUI>()
        ->GetProfileCustomizationHandlerForTesting()
        ->HandleDeleteProfile(base::ListValue());
  }

  // Returns profile picker webUI handler. Profile picker must be opened before
  // calling this function.
  ProfilePickerHandler* profile_picker_handler() {
    DCHECK(ProfilePicker::IsOpen());
    return web_contents()
        ->GetWebUI()
        ->GetController()
        ->GetAs<ProfilePickerUI>()
        ->GetProfilePickerHandlerForTesting();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  void SimulateNavigateBack() {
    // Use "Command [" for Mac and "Alt Left" for the other operating systems.
#if BUILDFLAG(IS_MAC)
    view()->AcceleratorPressed(
        ui::Accelerator(ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN));
#else
    view()->AcceleratorPressed(ui::Accelerator(ui::VKEY_LEFT, ui::EF_ALT_DOWN));
#endif
  }

 protected:
  const GURL kLocalProfileCreationUrl = AppendProfileCustomizationQueryParams(
      GURL("chrome://profile-customization"),
      ProfileCustomizationStyle::kLocalProfileCreation);
  const std::string kLocalProfileName = "LocalProfile";

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::CallbackListSubscription create_services_subscription_;
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      platform_management_;
#endif
  base::test::ScopedFeatureList feature_list_{
      switches::kChromeIdentitySurveySwitchProfileFromProfilePicker};
};

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest, ShowPicker) {
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kOnStartup));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  WaitForPickerWidgetCreated();
  // Check that non-default accessible title is provided both before the page
  // loads and after it loads.
  views::WidgetDelegate* delegate = widget()->widget_delegate();

  ui::AXNodeData root_view_data;
  widget()->GetRootView()->GetViewAccessibility().GetAccessibleNodeData(
      &root_view_data);
  EXPECT_EQ(
      root_view_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      delegate->GetAccessibleWindowTitle());
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
  WaitForLoadStop(GURL("chrome://profile-picker"));

  root_view_data = ui::AXNodeData();
  widget()->GetRootView()->GetViewAccessibility().GetAccessibleNodeData(
      &root_view_data);
  EXPECT_EQ(
      root_view_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      delegate->GetAccessibleWindowTitle());
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest, ShowChoice) {
  base::HistogramTester histogram_tester;
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  WaitForPickerWidgetCreated();
  // Check that non-default accessible title is provided both before the page
  // loads and after it loads.
  views::WidgetDelegate* delegate = widget()->widget_delegate();
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
  WaitForLoadStop(GURL("chrome://profile-picker/new-profile"));
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kUserManager, 1);
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileOpensUrlsOnPickProfile) {
  ProfilePicker::SetOpenCommandLineUrlsInNextProfileOpened(true);
  base::CommandLine::ForCurrentProcess()->AppendArg("https://www.google.com");
  base::CommandLine::ForCurrentProcess()->AppendArg("https://www.youtube.com");
  base::CommandLine::ForCurrentProcess()->AppendArg("https://www.gmail.com");

  ASSERT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  // Create a second profile.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* other_profile = profile_manager->GetProfile(other_path);

  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  // Open the other profile.
  BrowserAddedWaiter browser_waiter(2u, BrowserAddedWaiter::ReturnMode::kNew);
  OpenProfileFromPicker(other_path, /*open_settings=*/false);

  BrowserWindowInterface* const new_browser = browser_waiter.Wait();
  ASSERT_EQ(new_browser->GetProfile(), other_profile);
  ASSERT_EQ(4u, new_browser->GetTabStripModel()->count());
  ASSERT_FALSE(ProfilePicker::GetOpenCommandLineUrlsInNextProfileOpened());

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       OpenPickerAndClose) {
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  ProfilePicker::Hide();
  WaitForPickerClosed();
}

// Regression test for https://crbug.com/40764426.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       OpenPickerWhileClosing) {
  // Open the first picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // Request to open the second picker window while the first one is still
  // closing.
  ProfilePicker::Hide();
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileLocked));

  // The first picker should be closed and the second picker should be
  // displayed.
  WaitForPickerClosedAndReopenedImmediately();
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest, ReShow) {
  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // Show the picker with a different entry point, the picker is reused.
  base::WeakPtr<views::Widget> widget_weak = widget()->GetWeakPtr();
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileLocked));
  EXPECT_FALSE(widget_weak->IsClosed());
}

// TODO(crbug.com/426520088): Re-enable this flaky test.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       DISABLED_OpenProfile) {
  base::HistogramTester histogram_tester;

  auto scoped_iph_delay =
      AvatarToolbarButton::SetScopedIPHMinDelayAfterCreationForTesting(
          base::Seconds(0));
  ASSERT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  // Create a second profile.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* other_profile = profile_manager->GetProfile(other_path);
  MockHatsService* hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          other_profile, base::BindRepeating(&BuildMockHatsService)));

  // Verify that a HaTS survey is launched when the user switch profile with the
  // profile picker.
  EXPECT_CALL(
      *hats_service,
      LaunchDelayedSurvey(
          kHatsSurveyTriggerIdentitySwitchProfileFromProfilePicker, _, _,
          UnorderedElementsAre(
              Pair("Channel", _),
              Pair("Chrome Version", version_info::GetVersion().GetString()),
              Pair("Number of Chrome Profiles", "2"),
              Pair("Number of Google Accounts", "0"),
              Pair("Sign-in Status", "Signed Out"))))
      .Times(2);

  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  // Open the other profile.
  BrowserAddedWaiter browser_waiter(2u, BrowserAddedWaiter::ReturnMode::kNew);
  OpenProfileFromPicker(other_path, /*open_settings=*/false);
  // Attempt to open the profile twice, within the same picker. Simulates double
  // clicking on a profile card, not having any undesired effect.
  // Check: crbug.com/389726677.
  OpenProfileFromPicker(other_path, /*open_settings=*/false);

  // Browser for the profile is displayed.
  BrowserWindowInterface* const new_browser = browser_waiter.Wait();
  WaitForBrowserUrl(GURL("chrome://newtab/"),
                    new_browser->GetTabStripModel()->GetActiveWebContents());
  EXPECT_EQ(new_browser->GetProfile()->GetPath(), other_path);
  WaitForPickerClosed();
  // IPH is shown.
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown(new_browser));

  // FirstProfileTime.* histograms aren't recorded because the picker
  // is opened from the menu.
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "ProfilePicker.FirstProfileTime."),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       OpenProfileFromStartup) {
  base::HistogramTester histogram_tester;
  ASSERT_FALSE(ProfilePicker::IsOpen());

  // Create a second profile.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();

  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kOnStartup));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  WaitForLoadStop(GURL("chrome://profile-picker"));

  // Browser for the profile is displayed.
  BrowserAddedWaiter browser_waiter(2u, BrowserAddedWaiter::ReturnMode::kNew);

  // Open the new profile.
  OpenProfileFromPicker(other_path, /*open_settings=*/false);

  // Measurement of startup performance started.

  BrowserWindowInterface* const new_browser = browser_waiter.Wait();
  WaitForFirstNonEmptyPaint(
      GURL("chrome://newtab/"),
      new_browser->GetTabStripModel()->GetActiveWebContents());
  EXPECT_EQ(new_browser->GetProfile()->GetPath(), other_path);
  WaitForPickerClosed();

  if (base::FeatureList::IsEnabled(features::kWebUIReloadButton) &&
      features::kWebUIReloadButtonDeferBrowserViewShow.Get()) {
    // When kWebUIReloadButtonDeferBrowserViewShow is enabled, the browser
    // window is intentionally hidden initially, which causes
    // FirstWebContentsFinishReason to be kAbandonNoInitiallyVisibleContent and
    // prevents FirstWebContentsNonEmptyPaint from being recorded.
    histogram_tester.ExpectTotalCount(
        "ProfilePicker.FirstProfileTime.FirstWebContentsNonEmptyPaint", 0);
    histogram_tester.ExpectUniqueSample(
        "ProfilePicker.FirstProfileTime.FirstWebContentsFinishReason",
        metrics::StartupProfilingFinishReason::
            kAbandonNoInitiallyVisibleContent,
        1);
  } else {
    histogram_tester.ExpectTotalCount(
        "ProfilePicker.FirstProfileTime.FirstWebContentsNonEmptyPaint", 1);
    histogram_tester.ExpectUniqueSample(
        "ProfilePicker.FirstProfileTime.FirstWebContentsFinishReason",
        metrics::StartupProfilingFinishReason::kDone, 1);
  }
}

// TODO(crbug.com/40817459) Test is flaky on Linux CFI, Linux dbg, Mac ASan
#if ((BUILDFLAG(CFI_ICALL_CHECK) || !defined(NDEBUG)) && \
     BUILDFLAG(IS_LINUX)) ||                             \
    (BUILDFLAG(IS_MAC) && defined(ADDRESS_SANITIZER))
#define MAYBE_OpenProfile_Settings DISABLED_OpenProfile_Settings
#else
#define MAYBE_OpenProfile_Settings OpenProfile_Settings
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       MAYBE_OpenProfile_Settings) {
  auto scoped_iph_delay =
      AvatarToolbarButton::SetScopedIPHMinDelayAfterCreationForTesting(
          base::Seconds(0));
  ASSERT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  // Create a second profile.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();
  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  // Open the other profile.
  BrowserAddedWaiter browser_waiter(2u, BrowserAddedWaiter::ReturnMode::kNew);
  OpenProfileFromPicker(other_path, /*open_settings=*/true);
  // Browser for the profile is displayed.
  BrowserWindowInterface* const new_browser = browser_waiter.Wait();
  WaitForBrowserUrl(GURL("chrome://settings/manageProfile"),
                    new_browser->GetTabStripModel()->GetActiveWebContents());
  EXPECT_EQ(new_browser->GetProfile()->GetPath(), other_path);
  WaitForPickerClosed();
  // IPH is not shown.
  EXPECT_FALSE(ProfileSwitchPromoHasBeenShown(new_browser));
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       OpenURL_PickerClosed) {
  ASSERT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  const GURL kTargetURL("chrome://settings/help");
  // Create a profile.
  base::FilePath profile_path = CreateNewProfileWithoutBrowser();
  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::ForBackgroundManager(kTargetURL));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  // Open the profile.
  BrowserAddedWaiter browser_waiter(2u, BrowserAddedWaiter::ReturnMode::kNew);
  OpenProfileFromPicker(profile_path, /*open_settings=*/false);
  // Browser for the profile is displayed.
  BrowserWindowInterface* const new_browser = browser_waiter.Wait();
  WaitForBrowserUrl(kTargetURL,
                    new_browser->GetTabStripModel()->GetActiveWebContents());
  EXPECT_EQ(new_browser->GetProfile()->GetPath(), profile_path);
  WaitForPickerClosed();
}

// Regression test for https://crbug.com/40177639
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       OpenProfile_Guest) {
  auto scoped_iph_delay =
      AvatarToolbarButton::SetScopedIPHMinDelayAfterCreationForTesting(
          base::Seconds(0));
  ASSERT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  // Create a second profile.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();
  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  // Open a Guest profile.
  BrowserAddedWaiter browser_waiter(2u, BrowserAddedWaiter::ReturnMode::kNew);
  OpenGuestFromPicker();
  // Browser for the guest profile is displayed.
  BrowserWindowInterface* const new_browser = browser_waiter.Wait();
  WaitForBrowserUrl(GURL("chrome://newtab"),
                    new_browser->GetTabStripModel()->GetActiveWebContents());
  EXPECT_TRUE(new_browser->GetProfile()->IsGuestSession());
  WaitForPickerClosed();
  // IPH is not shown.
  EXPECT_FALSE(ProfileSwitchPromoHasBeenShown(new_browser));
}

// Closes the default browser window before creating a new profile in the
// profile picker.
// Regression test for https://crbug.com/40053746.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CloseBrowserBeforeCreatingNewProfile) {
  ASSERT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());

  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));

  // Close the browser window.
  ui_test_utils::BrowserDestroyedObserver observer(browser());
  chrome::CloseAllBrowsersWithProfile(browser()->GetProfile());
  observer.Wait();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());

  // Imitate creating a new profile through the profile picker.
  CreateLocalProfile();

  BrowserAddedWaiter(1u, BrowserAddedWaiter::ReturnMode::kNew).Wait();
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  WaitForPickerClosed();
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateLocalProfile) {
  ASSERT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  ASSERT_EQ(1u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());

  content::TestNavigationObserver profile_customization_observer(
      kLocalProfileCreationUrl);
  profile_customization_observer.StartWatchingNewWebContents();
  BrowserAddedWaiter waiter(2u, BrowserAddedWaiter::ReturnMode::kNew);

  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
  // Wait until webUI is fully initialized.
  WaitForLoadStop(GURL("chrome://profile-picker/new-profile"));

  // Simulate clicking the "Continue without an account" button.
  CreateLocalProfile();

  BrowserWindowInterface* const new_browser = waiter.Wait();
  profile_customization_observer.Wait();
  content::WebContents* dialog_web_contents =
      new_browser->GetFeatures()
          .signin_view_controller()
          ->GetModalDialogWebContentsForTesting();
  EXPECT_EQ(dialog_web_contents->GetLastCommittedURL(),
            kLocalProfileCreationUrl);

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(new_browser->GetProfile()->GetPath());
  ASSERT_TRUE(entry->IsEphemeral());
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_TRUE(
      new_browser->GetFeatures().signin_view_controller()->ShowsModalDialog());

  // Simulate clicking the "Done" button on the profile customization dialog.
  ConfirmLocalProfileCreation(dialog_web_contents);

  ASSERT_FALSE(entry->IsEphemeral());
  ASSERT_EQ(kLocalProfileName, base::UTF16ToUTF8(entry->GetLocalProfileName()));
  ASSERT_EQ(2u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());
  EXPECT_FALSE(
      new_browser->GetFeatures().signin_view_controller()->ShowsModalDialog());
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_CancelLocalProfileCreation DISABLED_CancelLocalProfileCreation
#else
#define MAYBE_CancelLocalProfileCreation CancelLocalProfileCreation
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       MAYBE_CancelLocalProfileCreation) {
  ASSERT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  ASSERT_EQ(1u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());

  content::TestNavigationObserver profile_customization_observer(
      kLocalProfileCreationUrl);
  profile_customization_observer.StartWatchingNewWebContents();
  BrowserAddedWaiter browser_added_waiter(2u,
                                          BrowserAddedWaiter::ReturnMode::kNew);

  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
  // Wait until webUI is fully initialized.
  WaitForLoadStop(GURL("chrome://profile-picker/new-profile"));

  // Simulate clicking the "Continue without an account" button.
  CreateLocalProfile();

  BrowserWindowInterface* const new_browser = browser_added_waiter.Wait();
  profile_customization_observer.Wait();
  content::WebContents* dialog_web_contents =
      new_browser->GetFeatures()
          .signin_view_controller()
          ->GetModalDialogWebContentsForTesting();
  EXPECT_EQ(dialog_web_contents->GetLastCommittedURL(),
            kLocalProfileCreationUrl);

  base::FilePath profile_path = new_browser->GetProfile()->GetPath();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  ASSERT_EQ(2u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());
  ASSERT_TRUE(entry->IsEphemeral());
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_TRUE(
      new_browser->GetFeatures().signin_view_controller()->ShowsModalDialog());

  // Simulate clicking the "Delete profile" button on the profile customization
  // dialog.
  ProfileDeletionObserver observer;
  DeleteLocalProfile(dialog_web_contents);
  observer.Wait();

  ASSERT_EQ(1u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());
  ASSERT_EQ(nullptr, g_browser_process->profile_manager()
                         ->GetProfileAttributesStorage()
                         .GetProfileAttributesWithPath(profile_path));
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest, DeleteProfile) {
  // Create a second profile.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();
  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(other_path);
  ASSERT_TRUE(profile);
  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  ProfilePickerHandler* handler = profile_picker_handler();

  // Simulate profile deletion from the picker.
  ProfileDestructionWaiter waiter(profile);
  base::ListValue args;
  args.Append(base::FilePathToValue(other_path));
  handler->HandleGetProfileStatistics(args);
  handler->HandleRemoveProfile(args);
  waiter.Wait();
}

// Regression test for https://crbug.com/40934491
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       DeleteProfileFromOwnTab) {
  // Open the picker in a tab.
  Profile* profile = browser()->GetProfile();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIProfilePickerUrl)));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ProfilePickerHandler* handler = contents->GetWebUI()
                                      ->GetController()
                                      ->GetAs<ProfilePickerUI>()
                                      ->GetProfilePickerHandlerForTesting();

  // Simulate profile deletion from the picker.
  ProfileDestructionWaiter waiter(profile);
  base::ListValue args;
  args.Append(base::FilePathToValue(profile->GetPath()));
  handler->HandleGetProfileStatistics(args);
  handler->HandleRemoveProfile(args);
  waiter.Wait();
}

class SupervisedProfilePickerHideGuestModeTest
    : public ProfilePickerCreationFlowBrowserTest {
 public:
  void OpenProfilePicker() {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
    WaitForLoadStop(GURL("chrome://profile-picker"));
  }

  void RemoveProfile(Profile* profile) {
    ProfileDestructionWaiter waiter(profile);
    webui::DeleteProfileAtPath(profile->GetPath(),
                               ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
    waiter.Wait();
  }

  bool IsGuestModeButtonHidden() {
    return content::EvalJs(
               web_contents(),
               base::StrCat({"getComputedStyle(", kBrowseAsGuestButtonPath,
                             ").display == \"none\" "}))
        .ExtractBool();
  }

  ::testing::AssertionResult ClickGuestModeButton() {
    return content::ExecJs(web_contents(),
                           base::StrCat({kBrowseAsGuestButtonPath, ".click"}));
  }

 private:
  static constexpr char kBrowseAsGuestButtonPath[] =
      "document.body.getElementsByTagName('"
      "profile-picker-app')[0].shadowRoot."
      "getElementById('mainView').shadowRoot.getElementById("
      "\'browseAsGuestButton\')";
};

IN_PROC_BROWSER_TEST_F(SupervisedProfilePickerHideGuestModeTest,
                       RegularProfile_GuestModeAvailable) {
  Profile* default_profile = browser()->GetProfile();
  AccountInfo default_account_info =
      FinishDiceSignIn(default_profile, "adult@gmail.com", "Adult");

  // Open the picker.
  OpenProfilePicker();

  EXPECT_FALSE(IsGuestModeButtonHidden());
  EXPECT_TRUE(ClickGuestModeButton());
}
