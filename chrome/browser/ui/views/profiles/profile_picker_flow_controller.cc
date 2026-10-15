// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_flow_controller.h"

#include <string>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/first_web_contents_profiler_base.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/signin/signin_hats_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_customization_util.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/prefs/pref_service.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/themes.mojom.h"

namespace {

// Returns the URL to load as initial content for the profile picker. If an
// empty URL is returned, the profile picker should not be shown until
// another explicit call with a non-empty URL given to the view
// (see `ProfilePickerView::ShowScreen()` for example)
GURL GetInitialURL(ProfilePicker::EntryPoint entry_point) {
  GURL base_url = GURL(chrome::kChromeUIProfilePickerUrl);
  switch (entry_point) {
    case ProfilePicker::EntryPoint::kOnStartup:
    case ProfilePicker::EntryPoint::kOnStartupNoProfile: {
      GURL::Replacements replacements;
      replacements.SetQueryStr(chrome::kChromeUIProfilePickerStartupQuery);
      return base_url.ReplaceComponents(replacements);
    }
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcess:
    case ProfilePicker::EntryPoint::kProfileLocked:
    case ProfilePicker::EntryPoint::kUnableToCreateBrowser:
    case ProfilePicker::EntryPoint::kBackgroundModeManager:
    case ProfilePicker::EntryPoint::kProfileIdle:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcessNoProfile:
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuManageProfiles:
      return base_url;
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile:
    case ProfilePicker::EntryPoint::kOnStartupCreateProfileWithEmail:
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuAddNewProfile:
      return base_url.Resolve("new-profile");
    case ProfilePicker::EntryPoint::kFirstRun:
    case ProfilePicker::EntryPoint::kOmniboxEverywhere:
      // Should not be used for this entry point.
      NOTREACHED();
  }
}

void RecordProfilingFinishReason(
    metrics::StartupProfilingFinishReason finish_reason) {
  base::UmaHistogramEnumeration(
      "ProfilePicker.FirstProfileTime.FirstWebContentsFinishReason",
      finish_reason);
}

class FirstWebContentsProfilerForProfilePicker
    : public metrics::FirstWebContentsProfilerBase {
 public:
  explicit FirstWebContentsProfilerForProfilePicker(
      content::WebContents* web_contents,
      base::TimeTicks pick_time);

  FirstWebContentsProfilerForProfilePicker(
      const FirstWebContentsProfilerForProfilePicker&) = delete;
  FirstWebContentsProfilerForProfilePicker& operator=(
      const FirstWebContentsProfilerForProfilePicker&) = delete;

 protected:
  // FirstWebContentsProfilerBase:
  void RecordFinishReason(
      metrics::StartupProfilingFinishReason finish_reason) override;
  void RecordNavigationFinished(base::TimeTicks navigation_start) override;
  void RecordFirstNonEmptyPaint() override;
  void RecordFirstNonEmptyPaintForOsLaunch() override;
  bool WasStartupInterrupted() override;

 private:
  ~FirstWebContentsProfilerForProfilePicker() override;

  const base::TimeTicks pick_time_;
};

FirstWebContentsProfilerForProfilePicker::
    FirstWebContentsProfilerForProfilePicker(content::WebContents* web_contents,
                                             base::TimeTicks pick_time)
    : FirstWebContentsProfilerBase(web_contents), pick_time_(pick_time) {
  DCHECK(!pick_time_.is_null());
}

FirstWebContentsProfilerForProfilePicker::
    ~FirstWebContentsProfilerForProfilePicker() = default;

void FirstWebContentsProfilerForProfilePicker::RecordFinishReason(
    metrics::StartupProfilingFinishReason finish_reason) {
  RecordProfilingFinishReason(finish_reason);
}

void FirstWebContentsProfilerForProfilePicker::RecordNavigationFinished(
    base::TimeTicks navigation_start) {
  // Nothing to record here for Profile Picker startups.
}

void FirstWebContentsProfilerForProfilePicker::RecordFirstNonEmptyPaint() {
  const char histogram_name[] =
      "ProfilePicker.FirstProfileTime.FirstWebContentsNonEmptyPaint";
  base::TimeTicks paint_time = base::TimeTicks::Now();
  base::UmaHistogramLongTimes100(histogram_name, paint_time - pick_time_);
  TRACE_EVENT_BEGIN("startup", histogram_name,
                    perfetto::Track::FromPointer(this), pick_time_);
  TRACE_EVENT_END("startup", perfetto::Track::FromPointer(this), paint_time);
}

void FirstWebContentsProfilerForProfilePicker::
    RecordFirstNonEmptyPaintForOsLaunch() {
  base::TimeTicks paint_time = base::TimeTicks::Now();
  base::UmaHistogramLongTimes100(
      "ProfilePicker.FirstProfileTime.FirstWebContentsNonEmptyPaint."
      "AutoLaunchByOs",
      paint_time - pick_time_);
}

bool FirstWebContentsProfilerForProfilePicker::WasStartupInterrupted() {
  // We're assuming that no interruptions block opening an existing profile
  // from the profile picker. We would detect this by observing really high
  // latency on the tracked metric, and can start tracking interruptions if we
  // find that such cases occur.
  return false;
}

// Measures time to display the first web contents.
void BeginFirstWebContentsProfiling(BrowserWindowInterface* browser,
                                    base::TimeTicks pick_time) {
  content::WebContents* visible_contents =
      metrics::FirstWebContentsProfilerBase::GetVisibleContents(browser);
  if (!visible_contents) {
    RecordProfilingFinishReason(metrics::StartupProfilingFinishReason::
                                    kAbandonNoInitiallyVisibleContent);
    return;
  }

  if (visible_contents->CompletedFirstVisuallyNonEmptyPaint()) {
    RecordProfilingFinishReason(
        metrics::StartupProfilingFinishReason::kAbandonAlreadyPaintedContent);
    return;
  }

  // FirstWebContentsProfilerForProfilePicker owns itself and is also bound to
  // |visible_contents|'s lifetime by observing WebContentsDestroyed().
  new FirstWebContentsProfilerForProfilePicker(visible_contents, pick_time);
}

void ShowLocalProfileCustomization(
    base::TimeTicks profile_picked_time_on_startup,
    BrowserWindowInterface* browser) {
  if (!browser) {
    // TODO(crbug.com/40242414): Make sure we do something or log an error if
    // opening a browser window was not possible.
    return;
  }

  DCHECK(browser->GetWindow());
  Profile* profile = browser->GetProfile();

  TRACE_EVENT1("browser", "ShowLocalProfileCustomization", "profile_path",
               profile->GetPath().AsUTF8Unsafe());

  if (!profile_picked_time_on_startup.is_null()) {
    BeginFirstWebContentsProfiling(browser, profile_picked_time_on_startup);
  }

  browser->GetFeatures()
      .signin_view_controller()
      ->ShowModalProfileCustomizationDialog(
          /*is_local_profile_creation=*/true);
}

void MaybeOpenPageInBrowser(BrowserWindowInterface* browser,
                            const GURL& target_page_url,
                            bool open_settings) {
  // User clicked 'Edit' from the profile card menu.
  if (open_settings) {
    chrome::ShowSettingsSubPage(browser, chrome::kManageProfileSubPage);
    return;
  }

  // If no url is provided, proceed with the normal profile startup tabs
  // behaviour.
  if (target_page_url.is_empty()) {
    return;
  }

  // Opens the target url upon user selecting a pre-existing profile.
  if (target_page_url.spec() == chrome::kChromeUIHelpURL) {
    chrome::ShowAboutChrome(browser);
  } else if (target_page_url.spec() == chrome::kChromeUISettingsURL) {
    chrome::ShowSettings(browser);
  } else if (target_page_url.spec() == ProfilePicker::kTaskManagerUrl) {
    chrome::OpenTaskManager(browser);
  } else {
    ShowSingletonTabOverwritingNTP(browser, target_page_url);
  }
}

}  // namespace

ProfilePickerFlowController::ProfilePickerFlowController(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    ProfilePicker::EntryPoint entry_point,
    const GURL& selected_profile_target_url)
    : ProfileManagementFlowControllerImpl(
          host,
          std::move(clear_host_callback),
          /*flow_type_string=*/"ProfilePickerFlow"),
      entry_point_(entry_point),
      selected_profile_target_url_(selected_profile_target_url) {}

ProfilePickerFlowController::~ProfilePickerFlowController() = default;

void ProfilePickerFlowController::Init() {
  RegisterStep(Step::kProfilePicker,
               ProfileManagementStepController::CreateForProfilePickerApp(
                   host(), GetInitialURL(entry_point_)));
  SwitchToStep(Step::kProfilePicker, /*reset_state=*/true);
}

void ProfilePickerFlowController::CancelSigninFlow() {
  // Triggered from either entreprise welcome or profile switch screens.
  DCHECK(current_step() == Step::kPostSignInFlow ||
         current_step() == Step::kAccountSelection);

  switch (entry_point_) {
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOnStartup:
    case ProfilePicker::EntryPoint::kOnStartupNoProfile:
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcess:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcessNoProfile:
    case ProfilePicker::EntryPoint::kProfileLocked:
    case ProfilePicker::EntryPoint::kUnableToCreateBrowser:
    case ProfilePicker::EntryPoint::kBackgroundModeManager:
    case ProfilePicker::EntryPoint::kProfileIdle:
    case ProfilePicker::EntryPoint::kOnStartupCreateProfileWithEmail: {
      SwitchToStep(Step::kProfilePicker, /*reset_state=*/true);
      UnregisterStep(Step::kPostSignInFlow);
      UnregisterStep(Step::kAccountSelection);
      return;
    }
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuAddNewProfile:
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile: {
      // This results in destroying `this`.
      ExitFlow();
      return;
    }
    case ProfilePicker::EntryPoint::kFirstRun:
    case ProfilePicker::EntryPoint::kOmniboxEverywhere:
      NOTREACHED() << "CancelSigninFlow() is not reachable from "
                      "this entry point";
  }
}

std::u16string ProfilePickerFlowController::GetFallbackAccessibleWindowTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_PROFILE_PICKER_MAIN_VIEW_TITLE);
}

void ProfilePickerFlowController::SwitchToSignedOutPostIdentityFlow(
    Profile* profile) {
  CHECK(profile);
  created_profile_ = profile->GetWeakPtr();
  CreateSignedOutFlowWebContents(created_profile_.get());

  HandleIdentityStepsCompleted(
      created_profile_.get(),
      PostHostClearedCallback(base::BindOnce(&ShowLocalProfileCustomization,
                                             profile_picked_time_on_startup_)),
      /*is_continue_callback=*/false);
}

void ProfilePickerFlowController::PickProfile(
    const base::FilePath& profile_path,
    ProfilePicker::ProfilePickingArgs args,
    base::OnceCallback<void(bool)> pick_profile_complete_callback) {
  if (args.should_record_startup_metrics &&
      // Avoid overriding the picked time if already recorded. This can happen
      // for example if multiple profiles are picked:
      // https://crbug.com/40207781.
      profile_picked_time_on_startup_.is_null()) {
    profile_picked_time_on_startup_ = base::TimeTicks::Now();
  }

  bool open_command_line_urls =
      ProfilePicker::GetOpenCommandLineUrlsInNextProfileOpened();
  ProfilePicker::SetOpenCommandLineUrlsInNextProfileOpened(false);

  base::OnceCallback<void(BrowserWindowInterface*)>
      switch_to_profile_complete_callback = base::BindOnce(
          &ProfilePickerFlowController::OnSwitchToProfileComplete,
          weak_ptr_factory_.GetWeakPtr(), args.open_settings,
          args.exit_flow_after_profile_picked,
          std::move(pick_profile_complete_callback));

  g_browser_process->profile_manager()->CreateProfileAsync(
      profile_path,
      base::BindOnce(&ProfilePickerFlowController::OnProfileLoadedForPicking,
                     weak_ptr_factory_.GetWeakPtr(), open_command_line_urls,
                     std::move(switch_to_profile_complete_callback)));
}

void ProfilePickerFlowController::OnSwitchToProfileComplete(
    bool open_settings,
    bool exit_flow_after_profile_picked,
    base::OnceCallback<void(bool)> pick_profile_complete_callback,
    BrowserWindowInterface* browser) {
  if (!browser || browser->IsDeleteScheduled()) {
    // The browser is destroyed or about to be destroyed.
    if (pick_profile_complete_callback) {
      std::move(pick_profile_complete_callback).Run(false);
    }
    return;
  }

  DCHECK(browser->GetWindow());
  if (pick_profile_complete_callback) {
    std::move(pick_profile_complete_callback).Run(true);
  }
  Profile* profile = browser->GetProfile();
  TRACE_EVENT1("browser",
               "ProfilePickerFlowController::OnSwitchToProfileComplete",
               "profile_path", profile->GetPath().AsUTF8Unsafe());

  // Measure startup time to display first web contents if the profile picker
  // was displayed on startup and if the initiating action is instrumented. For
  // example we don't record pick time for profile creations.
  if (!profile_picked_time_on_startup_.is_null()) {
    BeginFirstWebContentsProfiling(browser, profile_picked_time_on_startup_);
  }

  // Only show the profile switch IPH when the user clicked the card, and there
  // are multiple profiles.
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributes();
  int profile_count =
      std::ranges::count(entries, false, &ProfileAttributesEntry::IsOmitted);
  if (profile_count > 1 && !open_settings &&
      selected_profile_target_url_.is_empty()) {
    BrowserWindow::FromBrowser(browser)->MaybeShowProfileSwitchIPH();
  }

  if (profile->IsGuestSession()) {
    RecordProfilePickerAction(ProfilePickerAction::kLaunchGuestProfile);
  } else {
    // Launch a HaTS survey if the user intentionally switched profiles via the
    // profile picker accessed from the profile menu. This excludes first-run or
    // startup scenarios.
    if (entry_point_ == ProfilePicker::EntryPoint::kProfileMenuManageProfiles) {
      signin::LaunchHatsSurveyForProfile(
          kHatsSurveyTriggerIdentitySwitchProfileFromProfilePicker, profile);
    }
    RecordProfilePickerAction(
        open_settings
            ? ProfilePickerAction::kLaunchExistingProfileCustomizeSettings
            : ProfilePickerAction::kLaunchExistingProfile);
  }

  MaybeOpenPageInBrowser(browser, selected_profile_target_url_, open_settings);
  // Closes the Profile Picker.
  //
  // Making sure the flow has not already exited here is needed, because
  // potentially this specific flow can be run twice by the time the host was
  // cleared; e.g. by clicking quickly multiple times when picking a Profile
  // from the Profile Picker view.
  // Depending on the state of the first call, subsequent calls may result in
  // `BeginFirstWebContentsProfiling()` recording multiple metrics.
  // TODO(crbug.com/389887233): Investigate further how often this happens to
  // consider having a better architecture to avoid those issues with multiple
  // flow-exiting calls being executed at the same time.
  if (!HasFlowExited() && exit_flow_after_profile_picked) {
    ExitFlow();
  }
}

base::queue<ProfileManagementFlowController::Step>
ProfilePickerFlowController::RegisterPostIdentitySteps(
    PostHostClearedCallback post_host_cleared_callback) {
  CHECK(created_profile_);
  base::queue<ProfileManagementFlowController::Step> post_identity_steps;

  // TODO(crbug.com/40942098): Find another way to fetch the web contents.
  content::WebContents* web_contents = GetSignedOutFlowWebContents();
  CHECK(web_contents);

  auto search_engine_choice_step_completed = base::BindOnce(
      &ProfilePickerFlowController::AdvanceToNextPostIdentityStep,
      base::Unretained(this));
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(
          created_profile_.get());
  RegisterStep(
      Step::kSearchEngineChoice,
      ProfileManagementStepController::CreateForSearchEngineChoice(
          host(), search_engine_choice_dialog_service, web_contents,
          SearchEngineChoiceDialogService::EntryPoint::kProfileCreation,
          std::move(search_engine_choice_step_completed)));
  post_identity_steps.emplace(
      ProfileManagementFlowController::Step::kSearchEngineChoice);

  RegisterStep(
      Step::kFinishFlow,
      ProfileManagementStepController::CreateForFinishFlowAndRunInBrowser(
          host(),
          base::BindOnce(
              &ProfilePickerFlowController::FinishFlowAndRunInBrowser,
              base::Unretained(this), base::Unretained(created_profile_.get()),
              std::move(post_host_cleared_callback))));
  post_identity_steps.emplace(
      ProfileManagementFlowController::Step::kFinishFlow);

  return post_identity_steps;
}

void ProfilePickerFlowController::OnProfileLoadedForPicking(
    bool open_command_line_urls,
    base::OnceCallback<void(BrowserWindowInterface*)>
        pick_profile_complete_callback,
    Profile* profile) {
  CHECK(pick_profile_complete_callback);
  if (!profile) {
    std::move(pick_profile_complete_callback).Run(nullptr);
    return;
  }

  profiles::OpenBrowserWindowForProfile(
      std::move(pick_profile_complete_callback), /*always_create=*/false,
      /*is_new_profile=*/false, open_command_line_urls, profile);
}
