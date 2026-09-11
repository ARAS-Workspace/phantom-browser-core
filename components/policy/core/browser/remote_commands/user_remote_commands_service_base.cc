// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/remote_commands/user_remote_commands_service_base.h"

#include "base/time/default_clock.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/core/common/remote_commands/remote_commands_constants.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"

namespace policy {

UserRemoteCommandsServiceBase::UserRemoteCommandsServiceBase(
    CloudPolicyCore* core)
    : core_(core) {}
UserRemoteCommandsServiceBase::~UserRemoteCommandsServiceBase() = default;

void UserRemoteCommandsServiceBase::Init() {
  CHECK(core_);
  if (!core_->service()->IsInitializationComplete()) {
    cloud_policy_service_observer_.Observe(core_->service());
    return;
  }

  OnCloudPolicyServiceInitializationCompleted();
}

void UserRemoteCommandsServiceBase::
    OnCloudPolicyServiceInitializationCompleted() {
  cloud_policy_service_observer_.Reset();
  CHECK(core_);
  core_->StartRemoteCommandsService(GetFactory(),
                                    PolicyInvalidationScope::kUser);
}

void UserRemoteCommandsServiceBase::OnPolicyRefreshed(bool success) {}

void UserRemoteCommandsServiceBase::Shutdown() {
  cloud_policy_service_observer_.Reset();
}

}  // namespace policy
