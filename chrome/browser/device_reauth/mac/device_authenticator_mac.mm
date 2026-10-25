// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/device_authenticator_mac.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/device_reauth/mac/authenticator_mac.h"
#include "chrome/grit/branded_strings.h"
#include "components/device_reauth/device_authenticator.h"
#include "device/fido/mac/touch_id_context.h"
#include "ui/base/l10n/l10n_util.h"

DeviceAuthenticatorMac::DeviceAuthenticatorMac(
    std::unique_ptr<AuthenticatorMacInterface> authenticator,
    DeviceAuthenticatorProxy* proxy,
    const device_reauth::DeviceAuthParams& params)
    : DeviceAuthenticatorCommon(proxy,
                                params.GetAuthenticationValidityPeriod(),
                                params.GetAuthResultHistogram()),
      authenticator_(std::move(authenticator)) {}

DeviceAuthenticatorMac::~DeviceAuthenticatorMac() = default;

bool DeviceAuthenticatorMac::CanAuthenticateWithBiometrics() {
  bool is_available = authenticator_->CheckIfBiometricsAvailable();
  base::UmaHistogramBoolean("PasswordManager.CanUseBiometricsMac",
                            is_available);
  return is_available;
}

bool DeviceAuthenticatorMac::CanAuthenticateWithBiometricOrScreenLock() {
  // We check if we can authenticate strictly with biometrics first as this
  // function has important side effects such as logging metrics related to how
  // often users have biometrics available.
  if (CanAuthenticateWithBiometrics()) {
    return true;
  }

  // TODO(crbug.com/4555994): Add metrics logging for the only screen lock
  // available case.
  return authenticator_->CheckIfBiometricsOrScreenLockAvailable();
}

void DeviceAuthenticatorMac::Cancel() {
  touch_id_auth_context_ = nullptr;
  if (callback_) {
    // No code should be run after the callback as the callback could already be
    // destroying "this".
    std::move(callback_).Run(/*success=*/false);
  }
}

void DeviceAuthenticatorMac::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  // Callers must ensure that previous authentication is canceled.
  DCHECK(!callback_);
  if (!NeedsToAuthenticate()) {
    RecordAuthResultSkipped();
    // No code should be run after the callback as the callback could already be
    // destroying "this".
    std::move(callback).Run(/*success=*/true);
    return;
  }
  callback_ = std::move(callback);
  if (!CanAuthenticateWithBiometrics()) {
    // AuthenticateUserWithNonBiometrics runs a dialog with a nested run loop.
    // Post a task to run it asynchronously, so that it doesn't block the
    // current call stack. This prevents UaF in the callers if the page is
    // closed during the nested run loop. https://crbug.com/508289938
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DeviceAuthenticatorMac::AuthenticateWithNonBiometricsAsync,
            weak_ptr_factory_.GetWeakPtr(), message));
    return;
  }

  touch_id_auth_context_ = device::fido::mac::TouchIdContext::Create();
  touch_id_auth_context_->PromptTouchId(
      message,
      base::BindOnce(&DeviceAuthenticatorMac::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceAuthenticatorMac::AuthenticateWithNonBiometricsAsync(
    const std::u16string& message) {
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  bool success = authenticator_->AuthenticateUserWithNonBiometrics(
      l10n_util::GetStringFUTF16(IDS_PASSWORDS_AUTHENTICATION_PROMPT_PREFIX,
                                 message));
  if (weak_this) {
    weak_this->OnAuthenticationCompleted(success);
  }
}

void DeviceAuthenticatorMac::OnAuthenticationCompleted(bool success) {
  touch_id_auth_context_ = nullptr;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!callback_) {
    return;
  }
  RecordAuthenticationTimeIfSuccessful(success);
  // No code should be run after the callback as the callback could already be
  // destroying "this".
  std::move(callback_).Run(success);
}
