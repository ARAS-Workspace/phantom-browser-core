// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_FEATURES_H_
#define DEVICE_FIDO_PUBLIC_FEATURES_H_

#include "base/component_export.h"
#include "base/feature.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace device {

COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnActorCheck);

// These five feature flags control whether iCloud Keychain is the default
// mechanism for platform credential creation in different situations.
// "Active" means that the user is an active user of the profile authenticator,
// defined by having used it in the past 31 days. "Drive" means that the user
// is currently signed into iCloud Drive, which isn't iCloud Keychain
// (what's needed), but the closest approximation that we can detect.

COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForGoogle);
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForActiveWithDrive);
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForActiveWithoutDrive);
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForInactiveWithDrive);
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnICloudKeychainForInactiveWithoutDrive);

// Enable non-autofill sign-in UI for conditional mediation.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnAmbientSignin);

// Enables the WebAuthn Signal API for Windows Hello.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnHelloSignal);

// Support the WebAuthn Cross-device ("hybrid") fallback URL extension.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnCrossDeviceFallbackUrl);

// Enables support for the WebAuthenticationRemoteDesktopAllowedOrigins
// enterprise policy for Isolated Web Apps.
COMPONENT_EXPORT(FIDO_PUBLIC)
BASE_DECLARE_FEATURE(kWebAuthnIWARemoteDesktopAllowedOriginsPolicy);

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_FEATURES_H_
