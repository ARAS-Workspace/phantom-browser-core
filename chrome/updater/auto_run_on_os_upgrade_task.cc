// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/auto_run_on_os_upgrade_task.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/sequence_checker.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace updater {

AutoRunOnOsUpgradeTask::AutoRunOnOsUpgradeTask(
    UpdaterScope scope,
    scoped_refptr<PersistedData> persisted_data)
    : scope_(scope), persisted_data_(persisted_data) {}

AutoRunOnOsUpgradeTask::~AutoRunOnOsUpgradeTask() = default;

void AutoRunOnOsUpgradeTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasOSUpgraded()) {
    std::move(callback).Run();
    return;
  }

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&AutoRunOnOsUpgradeTask::RunOnOsUpgradeForApps, this,
                     persisted_data_->GetAppIds()),
      base::BindOnce(&AutoRunOnOsUpgradeTask::SetOSUpgraded, this)
          .Then(std::move(callback)));
}

void AutoRunOnOsUpgradeTask::RunOnOsUpgradeForApps(
    const std::vector<std::string>& app_ids) {
  std::ranges::for_each(
      app_ids, [&](const auto& app_id) { RunOnOsUpgradeForApp(app_id); });
}

size_t AutoRunOnOsUpgradeTask::RunOnOsUpgradeForApp(const std::string& app_id) {
  return 0;
}

bool AutoRunOnOsUpgradeTask::HasOSUpgraded() {
  return false;
}

void AutoRunOnOsUpgradeTask::SetOSUpgraded() {}

}  // namespace updater
