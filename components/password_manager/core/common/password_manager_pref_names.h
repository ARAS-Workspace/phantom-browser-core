// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_

#include "build/build_config.h"

namespace password_manager::prefs {

// Alphabetical list of preference names specific to the PasswordManager
// component.

// Boolean controlling whether websites and apps can automatically upgrade
// existing accounts to use passkeys when available.
//
// This pref doesn't have a policy mapped to it directly. Passkey creation in
// general can be disabled using `kCredentialsEnableService` and
// `kCredentialsEnablePasskeys`, in which case this pref's value is ignored.
inline constexpr char kAutomaticPasskeyUpgrades[] =
    "credentials_enable_automatic_passkey_upgrades";

// Boolean controlling whether the password manager allows automatic signing in
// through Credential Management API.
//
// IMPORTANT: This pref is neither queried nor updated on Android if the
// unified password manager is enabled.
// Use `password_manager_util::IsAutoSignInEnabled` to check
// the value of this setting instead.
inline constexpr char kCredentialsEnableAutosignin[] =
    "credentials_enable_autosignin";

// The value of this preference controls whether the Password Manager will save
// credentials. When it is false, it doesn't ask if you want to save passwords
// but will continue to fill passwords.
//
// IMPORTANT: This pref is neither queried nor updated on Android if the
// unified password manager is enabled.
// Use `password_manager_util::IsSavingPasswordsEnabled` to check the value of
// this setting instead.
inline constexpr char kCredentialsEnableService[] =
    "credentials_enable_service";

// The value of this preference controls whether the Password Manager will save
// passkeys. When it is false, GPM is not a candidate for saving passkeys.
// (Although users could still save into GPM by using a phone after scanning a
// QR code.) `kCredentialsEnableService` being false implies this too.
//
// IMPORTANT: This pref is neither queried nor updated on Android.
inline constexpr char kCredentialsEnablePasskeys[] =
    "credentials_enable_passkeys";

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Boolean pref controlled by the DeletingUndecryptablePasswordsEnabled policy.
// If set to false it blocks deleting undecryptable passwords, otherwise the
// deletion can happen.
inline constexpr char kDeletingUndecryptablePasswordsEnabled[] =
    "password_manager.deleteting_undecryptable_passwords_enabled";
#endif

// Boolean that indicated whether first run experience for the auto sign-in
// prompt was shown or not.
inline constexpr char kWasAutoSignInFirstRunExperienceShown[] =
    "profile.was_auto_sign_in_first_run_experience_shown";

// Boolean indicating whether Chrome should check whether the credentials
// submitted by the user were part of a leak.
inline constexpr char kPasswordLeakDetectionEnabled[] =
    "profile.password_manager_leak_detection";

// Boolean indicating whether users can mute (aka dismiss) alerts resulting from
// compromised credentials that were submitted by the user.
inline constexpr char kPasswordDismissCompromisedAlertEnabled[] =
    "profile.password_dismiss_compromised_alert";

// Boolean pref indicating whether password sharing is enabled. Enables both
// sending and receiving passwords.
inline constexpr char kPasswordSharingEnabled[] =
    "password_manager.password_sharing_enabled";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
// List of urls for which password manager is disabled/blocked.
inline constexpr char kPasswordManagerBlocklist[] =
    "password_manager.password_manager_blocklist";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

}  // namespace password_manager::prefs

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
