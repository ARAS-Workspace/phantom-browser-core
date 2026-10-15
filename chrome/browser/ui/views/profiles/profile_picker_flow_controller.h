// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "components/signin/public/base/signin_buildflags.h"

class BrowserWindowInterface;
class Profile;

class ProfilePickerFlowController : public ProfileManagementFlowControllerImpl {
 public:
  ProfilePickerFlowController(ProfilePickerWebContentsHost* host,
                              ClearHostClosure clear_host_callback,
                              ProfilePicker::EntryPoint entry_point,
                              const GURL& selected_profile_target_url);
  ~ProfilePickerFlowController() override;

  void Init() override;

  void CancelSigninFlow() override;

  std::u16string GetFallbackAccessibleWindowTitle() const override;

  // Switch to the flow that is shown when the user decides to create a profile
  // without signing in.
  void SwitchToSignedOutPostIdentityFlow(Profile* profile);

  // ProfileManagementFlowControllerImpl:
  void PickProfile(
      const base::FilePath& profile_path,
      ProfilePicker::ProfilePickingArgs args,
      base::OnceCallback<void(bool)> pick_profile_complete_callback) override;

 protected:
  // ProfileManagementFlowControllerImpl
  base::queue<ProfileManagementFlowController::Step> RegisterPostIdentitySteps(
      PostHostClearedCallback post_host_cleared_callback) override;

 private:
  // Callback after loading the profile but before opening the browser.
  void OnProfileLoadedForPicking(
      bool open_command_line_urls,
      base::OnceCallback<void(BrowserWindowInterface*)>
          pick_profile_complete_callback,
      Profile* profile);

  // Callback after loading the profile and opening the browser.
  void OnSwitchToProfileComplete(
      bool open_settings,
      bool exit_flow_after_profile_picked,
      base::OnceCallback<void(bool)> pick_profile_complete_callback,
      BrowserWindowInterface* browser);

  const ProfilePicker::EntryPoint entry_point_;
  const GURL selected_profile_target_url_;

  base::WeakPtr<Profile> created_profile_;

  // Time when the user picked a profile to open, to measure browser startup
  // performance. Only set when the picker is shown on startup.
  base::TimeTicks profile_picked_time_on_startup_;

  base::WeakPtrFactory<ProfilePickerFlowController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_
