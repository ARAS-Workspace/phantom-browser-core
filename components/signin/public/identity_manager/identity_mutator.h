// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MUTATOR_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MUTATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

namespace signin {
enum class ConsentLevel;
class AccountsMutator;
class AccountsCookieMutator;
class DeviceAccountsSynchronizer;

// IdentityMutator is the mutating interface for IdentityManager.
class IdentityMutator {
 public:
  IdentityMutator(
      std::unique_ptr<PrimaryAccountMutator> primary_account_mutator,
      std::unique_ptr<AccountsMutator> accounts_mutator,
      std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator,
      std::unique_ptr<DeviceAccountsSynchronizer> device_accounts_synchronizer);

  virtual ~IdentityMutator();

  // IdentityMutator is non-copyable, non-moveable.
  IdentityMutator(IdentityMutator&& other) = delete;
  IdentityMutator const& operator=(IdentityMutator&& other) = delete;

  IdentityMutator(const IdentityMutator& other) = delete;
  IdentityMutator const& operator=(const IdentityMutator& other) = delete;

  // Returns pointer to the object used to change the signed-in state of the
  // primary account, if supported on the current platform. Otherwise, returns
  // null.
  PrimaryAccountMutator* GetPrimaryAccountMutator();

  // Returns pointer to the object used to seed accounts and mutate state of
  // accounts' refresh tokens, if supported on the current platform. Otherwise,
  // returns null.
  AccountsMutator* GetAccountsMutator();

  // Returns pointer to the object used to manipulate the cookies stored and the
  // accounts associated with them. Guaranteed to be non-null.
  AccountsCookieMutator* GetAccountsCookieMutator();

  // Returns pointer to the object used to seed accounts information from the
  // device-level accounts. May be null if the system has no such notion.
  DeviceAccountsSynchronizer* GetDeviceAccountsSynchronizer();

 private:
  // PrimaryAccountMutator instance. May be null if mutation of the primary
  // account state is not supported on the current platform.
  std::unique_ptr<PrimaryAccountMutator> primary_account_mutator_;

  // AccountsMutator instance. May be null if mutation of accounts is not
  // supported on the current platform.
  std::unique_ptr<AccountsMutator> accounts_mutator_;

  // AccountsCookieMutator instance. Guaranteed to be non-null, as this
  // functionality is supported on all platforms.
  std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator_;

  // DeviceAccountsSynchronizer instance.
  std::unique_ptr<DeviceAccountsSynchronizer> device_accounts_synchronizer_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MUTATOR_H_
