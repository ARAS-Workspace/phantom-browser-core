// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
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
