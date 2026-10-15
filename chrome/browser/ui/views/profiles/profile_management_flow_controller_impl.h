// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_IMPL_H_

#include <memory>

#include "base/containers/queue.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"

class Profile;
class ProfilePickerWebContentsHost;
class ProfileManagementStepController;

// Allows sharing the logic for registering and connecting together
// identity-related profile management steps.
class ProfileManagementFlowControllerImpl
    : public ProfileManagementFlowController {
 public:
  ProfileManagementFlowControllerImpl(ProfilePickerWebContentsHost* host,
                                      ClearHostClosure clear_host_callback,
                                      std::string_view flow_type_string);
  ~ProfileManagementFlowControllerImpl() override;

 protected:
  // To be called when the sign-in and/or sync steps of the flow are completed
  // (or skipped), to proceed with additional steps or finish the flow.
  //
  // When `is_continue_callback` is true, the flow should finishing up
  // immediately so that `post_host_cleared_callback` can be executed, without
  // showing other steps.
  // Otherwise, the flow can display post-identity steps, passing
  // `post_host_cleared_callback` along so they can be executed at the right
  // time.
  void HandleIdentityStepsCompleted(
      Profile* profile,
      PostHostClearedCallback post_host_cleared_callback,
      bool is_continue_callback);

  // Register the steps that will be shown after the identity step. The steps
  // should be registered and pushed to the queue in the order in which they
  // should be displayed.
  virtual base::queue<ProfileManagementFlowController::Step>
  RegisterPostIdentitySteps(
      PostHostClearedCallback post_host_cleared_callback) = 0;

  // Switches to the step at the front of the `post_identity_steps_` queue if it
  // is not empty.
  void AdvanceToNextPostIdentityStep();

 private:
  // Move to the steps that come after the identity step.
  void SwitchToPostIdentitySteps(
      PostHostClearedCallback post_host_cleared_callback);

  // The list of steps that are added to the flow.
  // It is populated by the return value of `RegisterPostIdentitySteps` that
  // should be overridden by the derived class.
  // `post_identity_steps` being empty would mean that we either don't have any
  // steps in the flow or that the flow is done.
  base::queue<ProfileManagementFlowController::Step> post_identity_steps_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_IMPL_H_
