// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/cloud/user_policy_signin_service_util.h"

#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/network_change_notifier.h"

namespace policy {

bool IsSignoutEvent(const signin::PrimaryAccountChangeEvent& event) {
  return event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
         signin::PrimaryAccountChangeEvent::Type::kCleared;
}

bool IsAnySigninEvent(const signin::PrimaryAccountChangeEvent& event) {
  // TODO(crbug.com/40066949): Remove kSync usage after users are migrated to
  // kSignin only after kSync sunset. See ConsentLevel::kSync for more details.
  return event.GetEventTypeFor(signin::ConsentLevel::kSync) ==
             signin::PrimaryAccountChangeEvent::Type::kSet ||
         event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
             signin::PrimaryAccountChangeEvent::Type::kSet;
}

bool CanApplyPoliciesForSignedInUser(
    bool check_for_refresh_token,
    signin::ConsentLevel consent_level,
    signin::IdentityManager* identity_manager) {
  return (
      check_for_refresh_token
          ? identity_manager->HasPrimaryAccountWithRefreshToken(consent_level)
          : identity_manager->HasPrimaryAccount(consent_level));
}

}  // namespace policy
