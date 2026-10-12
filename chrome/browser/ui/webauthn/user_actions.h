// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_USER_ACTIONS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_USER_ACTIONS_H_

#include <vector>

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "device/fido/public/fido_constants.h"

namespace webauthn::user_actions {

// Emits what authenticators are visible to the user in the WebAuthn selection
// dialog when there are multiple options are available. Targets only GPM,
// iCloud Keychain and Windows Hello authenticators.
void RecordMultipleOptionsShown(
    const std::vector<AuthenticatorRequestDialogModel::Mechanism>& mechanisms,
    device::FidoRequestType request_type);

// Emits what authenticator is displayed as the priority mechanism in the
// priority WebAuthn credential selector dialog.
void RecordPriorityOptionShown(
    const AuthenticatorRequestDialogModel::Mechanism& mechanism);

// Emits when the combined selector is shown. This is used for get() requests
// with mediation "immediate".
void RecordCombinedSelectorShown();

void RecordSecurityKeyDialogShown(device::FidoRequestType request_type);

void RecordMechanismClick(
    const AuthenticatorRequestDialogModel::Mechanism& mech);

void RecordCancelClick();

void RecordAcceptClick();

void RecordICloudShown(device::FidoRequestType request_type);
void RecordICloudCancelled();
void RecordICloudSuccess();

void RecordChromeProfileAuthenticatorShown(
    device::FidoRequestType request_type);
void RecordChromeProfileCancelled();
void RecordChromeProfileSuccess();

void RecordWindowsHelloShown(device::FidoRequestType request_type);
void RecordWindowsHelloCancelled();
void RecordWindowsHelloSuccess();

void RecordContextMenuEntryClick();

}  // namespace webauthn::user_actions

#endif  // CHROME_BROWSER_UI_WEBAUTHN_USER_ACTIONS_H_
