// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/default_browser_step_eligibility_checker.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

}  // namespace

DefaultBrowserStepEligibilityChecker::DefaultBrowserStepEligibilityChecker() =
    default;

DefaultBrowserStepEligibilityChecker::~DefaultBrowserStepEligibilityChecker() =
    default;

void DefaultBrowserStepEligibilityChecker::CheckEligibility(
    Profile& profile,
    base::OnceCallback<void(bool)> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

void DefaultBrowserStepEligibilityChecker::StartCheckIsDefault(
    shell_integration::DefaultWebClientWorkerCallback callback) {
  base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
      ->StartCheckIsDefault(std::move(callback));
}

std::string DefaultBrowserStepEligibilityChecker::GetStepIdentifier() const {
  return std::string(kFeatureShowcaseDefaultBrowserStepIdentifier);
}

void DefaultBrowserStepEligibilityChecker::OnCheckFinished(
    base::OnceCallback<void(bool)> callback,
    shell_integration::DefaultWebClientState state) {
  std::move(callback).Run(state == shell_integration::NOT_DEFAULT ||
                          state == shell_integration::OTHER_MODE_IS_DEFAULT);
}
