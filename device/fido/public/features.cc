// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace device {

// Flags defined in this file should have a comment above them that either
// marks them as permanent flags, or specifies what lifecycle stage they're at.
//
// Permanent flags are those that we'll keep around indefinitely because
// they're useful for testing, debugging, etc. These should be commented with
//    // Permanent flag
//
// Standard flags progress through a lifecycle and are eliminated at the end of
// it. The comments above them should be one of the following:
//    // Not yet enabled by default.
//    // Enabled in M123. Remove in or after M126.
//
// Every release or so we should cleanup and delete flags which have been
// default-enabled for long enough, based on the removal milestone in their
// comment.

// This is used to enable an experiment to reject WebAuthn requests
// when actor mode is on.
BASE_FEATURE(kWebAuthnActorCheck, base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnICloudKeychainForGoogle,
             "WebAuthenticationICloudKeychainForGoogle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnICloudKeychainForActiveWithDrive,
             "WebAuthenticationICloudKeychainForActiveWithDrive",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnICloudKeychainForActiveWithoutDrive,
             "WebAuthenticationICloudKeychainForActiveWithoutDrive",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled in M118. Remove in or after M121.
BASE_FEATURE(kWebAuthnICloudKeychainForInactiveWithDrive,
             "WebAuthenticationICloudKeychainForInactiveWithDrive",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnICloudKeychainForInactiveWithoutDrive,
             "WebAuthenticationICloudKeychainForInactiveWithoutDrive",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Not yet enabled by default.
BASE_FEATURE(kWebAuthnAmbientSignin,
             "WebAuthenticationAmbientSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<WebAuthnAmbientSigninDisplay>::Option
    kWebAuthnAmbientSigninDisplayOptions[] = {
        {WebAuthnAmbientSigninDisplay::kSuggestionChip, "suggestion_chip"},
        {WebAuthnAmbientSigninDisplay::kAnchoredMessage, "anchored_message"},
};

// Suggestion chip is the default, but can be overridden from chrome://flags.
const base::FeatureParam<WebAuthnAmbientSigninDisplay>
    kWebAuthnAmbientSigninDisplayParam{
        &kWebAuthnAmbientSignin, "display",
        WebAuthnAmbientSigninDisplay::kSuggestionChip,
        &kWebAuthnAmbientSigninDisplayOptions};

// Deprecation flag. Disabled by default in M145. Remove in or after M148.

// Disabled by default.
BASE_FEATURE(kWebAuthnHelloSignal,
             "WebAuthenticationHelloSignal",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebAuthnCrossDeviceFallbackUrl,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled by default in M149. Remove in or after M152.
BASE_FEATURE(kWebAuthnIWARemoteDesktopAllowedOriginsPolicy,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace device
