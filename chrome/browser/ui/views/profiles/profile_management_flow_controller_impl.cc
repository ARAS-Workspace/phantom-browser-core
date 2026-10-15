// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"

ProfileManagementFlowControllerImpl::ProfileManagementFlowControllerImpl(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    std::string_view flow_type_string)
    : ProfileManagementFlowController(host,
                                      std::move(clear_host_callback),
                                      flow_type_string) {}

ProfileManagementFlowControllerImpl::~ProfileManagementFlowControllerImpl() =
    default;

void ProfileManagementFlowControllerImpl::SwitchToPostIdentitySteps(
    PostHostClearedCallback post_host_cleared_callback) {
  post_identity_steps_ =
      RegisterPostIdentitySteps(std::move(post_host_cleared_callback));
  AdvanceToNextPostIdentityStep();
}

void ProfileManagementFlowControllerImpl::AdvanceToNextPostIdentityStep() {
  if (post_identity_steps_.empty()) {
    return;
  }

  Step next_step = post_identity_steps_.front();
  post_identity_steps_.pop();
  SwitchToStep(next_step, /*reset_state=*/true);
}

void ProfileManagementFlowControllerImpl::HandleIdentityStepsCompleted(
    Profile* profile,
    PostHostClearedCallback post_host_cleared_callback,
    bool is_continue_callback) {
  CHECK(profile);

  if (is_continue_callback) {
    RegisterStep(
        Step::kFinishFlow,
        ProfileManagementStepController::CreateForFinishFlowAndRunInBrowser(
            host(),
            base::BindOnce(
                &ProfileManagementFlowControllerImpl::FinishFlowAndRunInBrowser,
                base::Unretained(this), profile,
                std::move(post_host_cleared_callback))));
    SwitchToStep(Step::kFinishFlow, /*reset_state=*/true);
    return;
  }

  SwitchToPostIdentitySteps(std::move(post_host_cleared_callback));
}
