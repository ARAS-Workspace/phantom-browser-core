// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_ui.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/regional_capabilities/regional_capabilities_metrics.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_controller.h"

namespace {
class ProfilePickerAppStepController : public ProfileManagementStepController {
 public:
  explicit ProfilePickerAppStepController(ProfilePickerWebContentsHost* host,
                                          const GURL& initial_url)
      : ProfileManagementStepController(host), initial_url_(initial_url) {}

  ~ProfilePickerAppStepController() override = default;

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    base::OnceClosure step_shown_success =
        base::BindOnce(std::move(step_shown_callback.value()), true);
    if (!loaded_ui_in_picker_contents_) {
      loaded_ui_in_picker_contents_ = true;
      host()->ShowScreenInPickerContents(initial_url_,
                                         std::move(step_shown_success));
      return;
    }

    if (reset_state) {
      // Don't do a full reset, just go back to the beginning of the history:
      host()->GetPickerContents()->GetController().GoToIndex(0);
    }
    host()->ShowScreenInPickerContents(GURL(), std::move(step_shown_success));
  }

  bool CanNavigateBack() const override {
    return CanNavigateBackInternal(host()->GetPickerContents());
  }

  void OnNavigateBackRequested() override {
    NavigateBackInternal(host()->GetPickerContents());
  }

 private:
  // We want to load the WebUI page exactly once, and do more lightweight
  // transitions if we need to switch back to this step later. So we track
  // whether the UI has been loaded or not.
  // Note that this relies on other steps not clearing the picker contents and
  // using their own instead.
  bool loaded_ui_in_picker_contents_ = false;
  const GURL initial_url_;
};

class FinishFlowAndRunInBrowserStepController
    : public ProfileManagementStepController {
 public:
  FinishFlowAndRunInBrowserStepController(
      ProfilePickerWebContentsHost* host,
      base::OnceClosure finish_flow_and_run_in_browser_callback)
      : ProfileManagementStepController(host),
        finish_flow_and_run_in_browser_callback_(
            std::move(finish_flow_and_run_in_browser_callback)) {
    CHECK(finish_flow_and_run_in_browser_callback_);
  }

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    CHECK(reset_state);
    CHECK(!step_shown_callback->is_null());
    std::move(step_shown_callback.value()).Run(true);
    std::move(finish_flow_and_run_in_browser_callback_).Run();
  }

 private:
  base::OnceClosure finish_flow_and_run_in_browser_callback_;
};

class SearchEngineChoiceStepController
    : public ProfileManagementStepController {
 public:
  SearchEngineChoiceStepController(
      ProfilePickerWebContentsHost* host,
      SearchEngineChoiceDialogService* search_engine_choice_dialog_service,
      content::WebContents* web_contents,
      SearchEngineChoiceDialogService::EntryPoint entry_point,
      base::OnceClosure step_completed_callback)
      : ProfileManagementStepController(host),
        entry_point_(entry_point),
        search_engine_choice_dialog_service_(
            search_engine_choice_dialog_service),
        step_completed_callback_(std::move(step_completed_callback)),
        web_contents_(web_contents) {
    CHECK(web_contents_);
  }

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    CHECK(reset_state);
    CHECK(!step_shown_callback->is_null());

    if (!search_engine_choice_dialog_service_) {
      // Mark that this step was skipped and proceed with the next one.
      std::move(step_shown_callback.value()).Run(false);
      std::move(step_completed_callback_).Run();
      return;
    }

    auto eligibility = search_engine_choice_dialog_service_
                           ->ComputeProfileManagementFlowConditions();

    // `IsChromeFirstRun()` is used here instead of checking the entry point
    // because the record is relative to whether this is the first run session,
    // not to the type of UI flow.
    regional_capabilities::RecordDebugTriggeringEligibility(
        eligibility, first_run::IsChromeFirstRun());

    if (base::FeatureList::IsEnabled(
            switches::kWaffleRestrictToAssociatedCountries)) {
      search_engine_choice_dialog_service_->RecordTriggeringEligibility(
          eligibility);
      if (!regional_capabilities::IsEligible(eligibility)) {
        // Mark that this step was skipped and proceed with the next one.
        std::move(step_shown_callback.value()).Run(false);
        std::move(step_completed_callback_).Run();
        return;
      }
    }

    base::OnceClosure navigation_finished_closure =
        base::BindOnce(&SearchEngineChoiceStepController::OnLoadFinished,
                       base::Unretained(this));
    // Notify the caller first.
    navigation_finished_closure =
        base::BindOnce(std::move(step_shown_callback.value()), true)
            .Then(std::move(navigation_finished_closure));

    search_engines::SearchEngineChoiceScreenEvents choice_screen_event =
        search_engines::SearchEngineChoiceScreenEvents::
            kProfileCreationChoiceScreenWasDisplayed;
    if (entry_point_ ==
        SearchEngineChoiceDialogService::EntryPoint::kFirstRunExperience) {
      choice_screen_event = search_engines::SearchEngineChoiceScreenEvents::
          kFreChoiceScreenWasDisplayed;
    }
    search_engine_choice_dialog_service_->RecordChoiceScreenEvent(
        choice_screen_event);

    host()->ShowScreen(web_contents_,
                       GURL(chrome::kChromeUISearchEngineChoiceURL),
                       std::move(navigation_finished_closure));
  }

 private:
  void OnLoadFinished() {
    auto* search_engine_choice_ui = web_contents_->GetWebUI()
                                        ->GetController()
                                        ->GetAs<SearchEngineChoiceUI>();
    CHECK(search_engine_choice_ui);
    CHECK(step_completed_callback_);
    search_engine_choice_ui->Initialize(
        /*display_dialog_callback=*/base::OnceClosure(),
        /*on_choice_made_callback=*/
        std::move(step_completed_callback_), entry_point_);
  }

  // The entry point from which the search engine choice screen is displayed.
  // This could be either the FRE or Profile Creation in this case.
  SearchEngineChoiceDialogService::EntryPoint entry_point_;

  // Can be nullptr.
  raw_ptr<SearchEngineChoiceDialogService> search_engine_choice_dialog_service_;

  // Callback to be executed when the step is completed.
  base::OnceClosure step_completed_callback_;

  // The web contents in which we want to display the screen.
  raw_ptr<content::WebContents> web_contents_;
};
}  // namespace

// static
std::unique_ptr<ProfileManagementStepController>
ProfileManagementStepController::CreateForProfilePickerApp(
    ProfilePickerWebContentsHost* host,
    const GURL& initial_url) {
  return std::make_unique<ProfilePickerAppStepController>(host, initial_url);
}

// static
std::unique_ptr<ProfileManagementStepController>
ProfileManagementStepController::CreateForSearchEngineChoice(
    ProfilePickerWebContentsHost* host,
    SearchEngineChoiceDialogService* search_engine_choice_dialog_service,
    content::WebContents* web_contents,
    SearchEngineChoiceDialogService::EntryPoint entry_point,
    base::OnceClosure callback) {
  return std::make_unique<SearchEngineChoiceStepController>(
      host, search_engine_choice_dialog_service, web_contents, entry_point,
      std::move(callback));
}

// static
std::unique_ptr<ProfileManagementStepController>
ProfileManagementStepController::CreateForFinishFlowAndRunInBrowser(
    ProfilePickerWebContentsHost* host,
    base::OnceClosure finish_flow_and_run_in_browser_callback) {
  return std::make_unique<FinishFlowAndRunInBrowserStepController>(
      host, std::move(finish_flow_and_run_in_browser_callback));
}

ProfileManagementStepController::ProfileManagementStepController(
    ProfilePickerWebContentsHost* host)
    : host_(host) {}

ProfileManagementStepController::~ProfileManagementStepController() = default;

void ProfileManagementStepController::OnReloadRequested() {}

void ProfileManagementStepController::ToggleMediaEffects(bool active) {}

void ProfileManagementStepController::OnNavigateBackRequested() {
  NOTREACHED();
}

bool ProfileManagementStepController::CanNavigateBack() const {
  return false;
}

bool ProfileManagementStepController::CanNavigateBackInternal(
    content::WebContents* contents) const {
  return (contents && contents->GetController().CanGoBack()) || CanPopStep();
}

void ProfileManagementStepController::NavigateBackInternal(
    content::WebContents* contents) {
  if (contents && contents->GetController().CanGoBack()) {
    contents->GetController().GoBack();
    return;
  }

  if (CanPopStep()) {
    DCHECK(pop_step_callback_);
    std::move(pop_step_callback_).Run();
  }
}

bool ProfileManagementStepController::CanPopStep() const {
  return !pop_step_callback_.is_null();
}
