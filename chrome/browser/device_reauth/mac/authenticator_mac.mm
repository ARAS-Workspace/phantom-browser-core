// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/authenticator_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#include <Security/Security.h>

#include "base/apple/bridging.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/mac/authorization_util.h"
#include "base/mac/scoped_authorizationref.h"
#include "base/strings/sys_string_conversions.h"

namespace {

NSString* UserAuthenticationRightName() {
  // The authentication right name is of the form
  // `org.chromium.Chromium.access-passwords` or
  // `com.google.Chrome.access-passwords`.
  return [[base::apple::MainBundle() bundleIdentifier]
      stringByAppendingString:@".access-passwords"];
}

bool EnsureAuthorizationRightExists() {
  NSString* right_name = UserAuthenticationRightName();
  base::apple::ScopedCFTypeRef<CFDictionaryRef> right_definition;
  if (AuthorizationRightGet(right_name.UTF8String,
                            right_definition.InitializeInto()) ==
      errAuthorizationSuccess) {
    id rule = [base::apple::CFToNSPtrCast(right_definition.get())
        objectForKey:@(kAuthorizationRightRule)];
    if (NSString* rule_string = base::apple::ObjCCast<NSString>(rule);
        [rule_string
            isEqualToString:@(kAuthorizationRuleAuthenticateAsSessionUser)]) {
      return true;
    }
    if (NSArray* rule_array = base::apple::ObjCCast<NSArray>(rule); [rule_array
            containsObject:@(kAuthorizationRuleAuthenticateAsSessionUser)]) {
      return true;
    }
  }

  // The authorization right does not exist or does not match the desired rule,
  // so create or overwrite it.
  base::mac::ScopedAuthorizationRef authorization =
      base::mac::CreateAuthorization();
  if (!authorization) {
    return false;
  }

  // Create a right which requires that the user authenticate as the session
  // owner. The prompt must be specified each time the right is requested.
  OSStatus status =
      AuthorizationRightSet(authorization.get(), right_name.UTF8String,
                            CFSTR(kAuthorizationRuleAuthenticateAsSessionUser),
                            nullptr, nullptr, nullptr);
  if (status != errAuthorizationSuccess) {
    OSSTATUS_LOG(ERROR, status) << "AuthorizationRightSet";
    return false;
  }

  return true;
}

// Attempts to (re-)authenticate the user of the OS account. Returns true if
// the user was successfully authenticated.
bool AuthenticateUser(std::u16string prompt_string) {
  if (!EnsureAuthorizationRightExists()) {
    return false;
  }

  NSString* rightName = UserAuthenticationRightName();
  AuthorizationItem right_items[] = {{rightName.UTF8String, 0, nullptr, 0}};
  AuthorizationRights rights = {std::size(right_items), right_items};

  base::apple::ScopedCFTypeRef<CFStringRef> prompt =
      base::SysUTF16ToCFStringRef(prompt_string);

  // Pass kAuthorizationFlagDestroyRights to prevent the OS from saving the
  // authorization and not prompting the user when future requests are made.
  base::mac::ScopedAuthorizationRef authorization =
      base::mac::GetAuthorizationRightsWithPrompt(
          &rights, prompt.get(), kAuthorizationFlagDestroyRights);
  return static_cast<bool>(authorization);
}

}  // namespace

AuthenticatorMac::AuthenticatorMac() = default;

AuthenticatorMac::~AuthenticatorMac() = default;

bool AuthenticatorMac::CheckIfBiometricsAvailable() {
  LAContext* context = [[LAContext alloc] init];
  // FYI, these two LAPolicy constants are defined to be the same value (4), but
  // because the new constant has an annotation of macOS 15+ and the old
  // constant is marked as being deprecated, a runtime switch is forced.
  if (@available(macOS 15, *)) {
    return
        [context canEvaluatePolicy:
                     LAPolicyDeviceOwnerAuthenticationWithBiometricsOrCompanion
                             error:nil];
  } else {
    return [context
        canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometricsOrWatch
                    error:nil];
  }
}

bool AuthenticatorMac::CheckIfBiometricsOrScreenLockAvailable() {
  LAContext* context = [[LAContext alloc] init];
  return [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthentication
                              error:nil];
}

bool AuthenticatorMac::AuthenticateUserWithNonBiometrics(
    const std::u16string& message) {
  return AuthenticateUser(message);
}
