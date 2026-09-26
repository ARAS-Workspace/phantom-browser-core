// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/pref_names.h"
#include "components/permissions/permission_actions_history.h"
#include "components/pref_registry/pref_registry_syncable.h"

#include "build/build_config.h"

namespace permissions {
namespace prefs {

// List containing a history of past permission actions, for all permission
// types.
const char kPermissionActions[] = "profile.content_settings.permission_actions";

// The number of one time permission prompts a user has seen.
const char kOneTimePermissionPromptsDecidedCount[] =
    "profile.one_time_permission_prompts_decided_count";

}  // namespace prefs

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  PermissionActionsHistory::RegisterProfilePrefs(registry);
}

}  // namespace permissions
