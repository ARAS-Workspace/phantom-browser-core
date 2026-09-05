// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_RESET_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_RESET_SETTINGS_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_registry_simple.h"

class Profile;

namespace settings {

// Handler for
//  1) 'Reset Profile Settings' dialog
//  2) 'Powerwash' dialog (ChromeOS only)
class ResetSettingsHandler : public SettingsPageUIHandler {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  // Hash used by the Chrome Cleanup Tool when launching chrome with the reset
  // profile settings URL.
  static const char kCctResetSettingsHash[];

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
#endif  // BUILDFLAG(IS_CHROMEOS)

  static bool ShouldShowResetProfileBanner(Profile* profile);

  explicit ResetSettingsHandler(Profile* profile);

  ResetSettingsHandler(const ResetSettingsHandler&) = delete;
  ResetSettingsHandler& operator=(const ResetSettingsHandler&) = delete;

  ~ResetSettingsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override;

  // Retrieves the paths of tampered preferences.
  void HandleGetTamperedPreferencePaths(const base::ListValue& args);

 protected:
  // Overridden in tests to substitute with a test version of ProfileResetter.
  virtual ProfileResetter* GetResetter();

  // Javascript callback to start clearing data.
  void HandleResetProfileSettings(const base::ListValue& args);

 private:
  // Called when the reset profile banner is shown.
  void OnHideResetProfileBanner(const base::ListValue& args);

  // Retrieve the triggered reset tool name, called from Javascript.
  void HandleGetTriggeredResetToolName(const base::ListValue& args);

  // Resets the settings that are marked in the resettable flags to the default
  // value, callback will be called once the reset is complete. The difference
  // between this function and |ResetProfile| function is that individual
  // settings could be reset with this function.
  void ResetSettings(ProfileResetter::ResettableFlags resettable_flags,
                     base::OnceClosure callback);

  // Resets all profile settings to default values.
  void ResetProfile(const std::string& callback_id);

  // Closes the dialog once all requested settings has been reset.
  void OnResetProfileSettingsDone(std::string callback_id);

#if BUILDFLAG(IS_CHROMEOS)
  void OnShowSanitizeDialog(const base::ListValue& args);
#endif  // BUILDFLAG(IS_CHROMEOS)

  const raw_ptr<Profile> profile_;

  std::unique_ptr<ProfileResetter> resetter_;

  // Used to cancel callbacks when JavaScript becomes disallowed.
  base::WeakPtrFactory<ResetSettingsHandler> callback_weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_RESET_SETTINGS_HANDLER_H_
