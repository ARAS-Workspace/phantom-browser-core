// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_SERVICE_STARTUP_STATE_OBSERVER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_SERVICE_STARTUP_STATE_OBSERVER_H_

#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "components/signin/public/identity_manager/account_info.h"

class Profile;

namespace syncer {
class SyncService;
}  // namespace syncer

// Kill switch for enabling an observer that awaits the sync engine startup,
// before displaying the sync confirmation screen.
BASE_DECLARE_FEATURE(kEnableAwaitSyncServiceStartupOnHistorySync);

// Helper class to track the state of the SyncService.
// Executes a callback when the SyncService's state is no longer pending. The
// right implementation of the class is chosen based on the state of the
// `syncer::kEnableAwaitSyncServiceStartup` and
// `syncer::kReplaceSyncPromosWithSignInPromos` feature flags.
class SyncServiceStartupStateObserver {
 public:
  SyncServiceStartupStateObserver();
  virtual ~SyncServiceStartupStateObserver();

  static std::unique_ptr<SyncServiceStartupStateObserver>
  MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
      syncer::SyncService* sync_service,
      Profile* profile,
      const CoreAccountInfo& account_info,
      base::TimeDelta startup_delay,
      base::OnceClosure callback);

  // Public for testing.
  virtual void MockTimeoutReachedForTesting() = 0;
  virtual void OnSyncStartupStateChangedForTesting(
      SyncStartupTracker::ServiceStartupState state) = 0;

  // Callbacks to record any necessary metrics when the sync service
  // is ready to start or when we timeout waiting.
  void SetSyncStartupCompleteMetricsCallback(
      base::OnceCallback<void(base::TimeDelta)> callback);
  void SetTimeoutMetricsCallback(
      base::OnceCallback<void(base::TimeDelta)> callback);

 protected:
  base::OnceCallback<void(base::TimeDelta)>
      sync_startup_complete_metrics_callback_;
  base::OnceCallback<void(base::TimeDelta)> timeout_metrics_callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_SERVICE_STARTUP_STATE_OBSERVER_H_
