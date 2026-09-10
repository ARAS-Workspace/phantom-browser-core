// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/check_updater_health_task.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"

namespace updater {

CheckUpdaterHealthTask::CheckUpdaterHealthTask(UpdaterScope scope)
    : scope_(scope) {}
CheckUpdaterHealthTask::~CheckUpdaterHealthTask() = default;

void CheckUpdaterHealthTask::CheckAndRecordUpdaterHealth(
    const base::Version& version) {
  base::UmaHistogramBoolean("GoogleUpdate.UpdaterHealth.UpdaterValid",
                            version.IsValid());
  if (!version.IsValid()) {
    return;
  }

}

void CheckUpdaterHealthTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BrowserUpdaterClient::Create(scope_)->GetUpdaterVersion(
      base::BindPostTask(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
          base::BindOnce(&CheckUpdaterHealthTask::CheckAndRecordUpdaterHealth,
                         this))
          .Then(std::move(callback)));
}

}  // namespace updater
