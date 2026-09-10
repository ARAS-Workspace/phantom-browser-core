// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_manager.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/default_browser_monitor.h"
#include "chrome/browser/default_browser/default_browser_notification_observer.h"
#include "chrome/browser/default_browser/setters/default_browser_visual_guided_setter.h"
#include "chrome/browser/default_browser/setters/shell_integration_default_browser_setter.h"
#include "chrome/browser/shell_integration.h"
#include "url/gurl.h"

namespace default_browser {

namespace {

class ShellDelegateImpl : public DefaultBrowserManager::ShellDelegate {
 public:
  ShellDelegateImpl() = default;
  ~ShellDelegateImpl() override = default;
  ShellDelegateImpl(const ShellDelegateImpl&) = delete;
  ShellDelegateImpl& operator=(const ShellDelegateImpl&) = delete;

  void StartCheckIsDefault(
      shell_integration::DefaultWebClientWorkerCallback callback) override {
    auto worker =
        base::MakeRefCounted<shell_integration::DefaultBrowserWorker>();
    worker->StartCheckIsDefault(std::move(callback));
  }

};

// UMA enum for logging browser state validation result.
//
// LINT.IfChange(DefaultBrowserStateValidationResult)
enum class DefaultBrowserStateValidationResult {
  kTruePositive = 0,
  kTrueNegative = 1,
  kFalsePositive = 2,
  kFalseNegative = 3,
  kMaxValue = kFalseNegative
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:DefaultBrowserStateValidationResult)

}  // namespace

DefaultBrowserManager::ShellDelegate::~ShellDelegate() = default;

DEFINE_USER_DATA(DefaultBrowserManager);

DefaultBrowserManager::DefaultBrowserManager(
    BrowserProcess* browser_process,
    std::unique_ptr<ShellDelegate> shell_delegate,
    ProfileProviderCallback profile_provider_callback,
    std::unique_ptr<DefaultBrowserMonitor> monitor)
    : shell_delegate_(std::move(shell_delegate)),
      profile_provider_callback_(std::move(profile_provider_callback)),
      scoped_unowned_user_data_(browser_process->GetUnownedUserDataHost(),
                                *this) {
  CHECK(!profile_provider_callback_.is_null());
  if (IsDefaultBrowserFrameworkEnabled()) {
    monitor_ = monitor ? std::move(monitor)
                       : std::make_unique<DefaultBrowserMonitor>();

    monitor_subscription_ = monitor_->RegisterDefaultBrowserChanged(
        base::BindRepeating(&DefaultBrowserManager::OnMonitorDetectedChange,
                            base::Unretained(this)));
    if (IsDefaultBrowserChangedOsNotificationEnabled()) {
      notification_observer_ =
          std::make_unique<DefaultBrowserNotificationObserver>(
              base::BindOnce(
                  &DefaultBrowserManager::RegisterDefaultBrowserChanged,
                  base::Unretained(this)),
              base::BindOnce(&DefaultBrowserManager::GetDefaultBrowserState,
                             base::Unretained(this)),
              *this);
    }
  }
}

DefaultBrowserManager::~DefaultBrowserManager() = default;

// static
DefaultBrowserManager* DefaultBrowserManager::From(
    BrowserProcess* browser_process) {
  return browser_process ? Get(browser_process->GetUnownedUserDataHost())
                         : nullptr;
}

// static
std::unique_ptr<DefaultBrowserManager::ShellDelegate>
DefaultBrowserManager::CreateDefaultDelegate() {
  return std::make_unique<ShellDelegateImpl>();
}

// static
std::unique_ptr<DefaultBrowserController>
DefaultBrowserManager::CreateControllerFor(
    DefaultBrowserEntrypointType entrypoint) {
  std::unique_ptr<DefaultBrowserSetter> setter;
  switch (GetDefaultBrowserSetterType()) {
    case DefaultBrowserSetterType::kShellIntegration:
      setter = std::make_unique<ShellIntegrationDefaultBrowserSetter>();
      break;
    case DefaultBrowserSetterType::kVisualGuide:
      setter = std::make_unique<DefaultBrowserVisualGuidedSetter>(
          DefaultBrowserManager::From(g_browser_process)->GetProfile());
      break;
    default:
      NOTREACHED();
  }

  return std::make_unique<DefaultBrowserController>(std::move(setter),
                                                    entrypoint);
}

Profile& DefaultBrowserManager::GetProfile() {
  return *profile_provider_callback_.Run();
}

void DefaultBrowserManager::GetDefaultBrowserState(
    DefaultBrowserCheckCompletionCallback callback) {
  shell_delegate_->StartCheckIsDefault(
      base::BindOnce(&DefaultBrowserManager::OnDefaultBrowserCheckResult,
                     base::Unretained(this), std::move(callback)));
}

void DefaultBrowserManager::OnDefaultBrowserCheckResult(
    DefaultBrowserCheckCompletionCallback callback,
    DefaultBrowserState default_state) {
  if (default_state == shell_integration::IS_DEFAULT ||
      default_state == shell_integration::NOT_DEFAULT) {
    if (base::FeatureList::IsEnabled(kPerformDefaultBrowserCheckValidations)) {
      PerformDefaultBrowserCheckValidations(default_state);
    }
  }

  std::move(callback).Run(default_state);
}

void DefaultBrowserManager::PerformDefaultBrowserCheckValidations(
    DefaultBrowserState default_state) {
}

base::CallbackListSubscription
DefaultBrowserManager::RegisterDefaultBrowserChanged(
    DefaultBrowserChangedCallback callback) {
  if (monitor_) {
    monitor_->StartMonitor();
  }
  return observers_.Add(std::move(callback));
}

void DefaultBrowserManager::OnMonitorDetectedChange() {
  GetDefaultBrowserState(base::BindOnce(&DefaultBrowserManager::NotifyObservers,
                                        base::Unretained(this)));
}

void DefaultBrowserManager::NotifyObservers(DefaultBrowserState state) {
  observers_.Notify(state);
}

void DefaultBrowserManager::TrackTimeAfterSetterFailure(
    DefaultBrowserEntrypointType entrypoint,
    DefaultBrowserSetterType setter) {
  tracker_subscription_ = {};
  tracker_ =
      std::make_unique<TimeAfterSetterFailureTracker>(entrypoint, setter);

  tracker_subscription_ = RegisterDefaultBrowserChanged(base::BindRepeating(
      &DefaultBrowserManager::OnDefaultBrowserStateChangedForTracker,
      base::Unretained(this)));

  GetDefaultBrowserState(base::BindOnce(
      &DefaultBrowserManager::OnDefaultBrowserStateChangedForTracker,
      base::Unretained(this)));
}

void DefaultBrowserManager::OnDefaultBrowserStateChangedForTracker(
    DefaultBrowserState state) {
  if (tracker_ && state == DefaultBrowserState::IS_DEFAULT) {
    tracker_->OnDefaultBrowserSet();
    tracker_subscription_ = {};
    tracker_.reset();
  }
}

}  // namespace default_browser
