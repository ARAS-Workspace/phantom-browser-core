// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_service_startup_state_observer.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "base/notreached.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace {
base::TimeDelta GetElapsedTime(const base::OneShotTimer& timer) {
  CHECK(timer.IsRunning());
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks start_time =
      timer.desired_run_time() - timer.GetCurrentDelay();
  return now - start_time;
}

bool AccountMayHaveCloudPolicies(Profile* profile, const std::string& email) {
  return signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
             email) ||
         policy::ManagementServiceFactory::GetForProfile(profile)
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD) ||
         policy::ManagementServiceFactory::GetForProfile(profile)
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD_DOMAIN) ||
         policy::ManagementServiceFactory::GetForPlatform()
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD) ||
         policy::ManagementServiceFactory::GetForPlatform()
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
}

bool IsSyncStartupInPendingState(syncer::SyncService* sync_service) {
  if (!sync_service) {
    return false;
  }
  auto transport_state = sync_service->GetTransportState();
  switch (transport_state) {
    case syncer::SyncService::TransportState::DISABLED:
    case syncer::SyncService::TransportState::PAUSED:
      return false;
    case syncer::SyncService::TransportState::START_DEFERRED:
    case syncer::SyncService::TransportState::INITIALIZING:
      return true;
    case syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case syncer::SyncService::TransportState::CONFIGURING:
    case syncer::SyncService::TransportState::ACTIVE:
      return false;
  }
  NOTREACHED();
}

// An implementation of `SyncServiceStartupStateObserver` directly
// observing the Sync Service and tracking if its transport state
// has reached a final state.
class SyncServiceStartupStateObserverImpl
    : public SyncServiceStartupStateObserver,
      public syncer::SyncServiceObserver {
 public:
  SyncServiceStartupStateObserverImpl(
      syncer::SyncService* sync_service,
      base::TimeDelta startup_delay,
      base::OnceClosure on_state_updated_callback);
  ~SyncServiceStartupStateObserverImpl() override;

  static std::unique_ptr<SyncServiceStartupStateObserver>
  MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
      syncer::SyncService* sync_service,
      Profile* profile,
      const CoreAccountInfo& account_info,
      base::TimeDelta startup_delay,
      base::OnceClosure callback);

  // SyncServiceStartupStateObserver implementation:
  void MockTimeoutReachedForTesting() override;  // IN-TEST
  void OnSyncStartupStateChangedForTesting(      // IN-TEST
      SyncStartupTracker::ServiceStartupState state) override;

 private:
  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  void OnSyncServiceStartupTimeout();

  base::OnceClosure on_state_updated_callback_;
  base::OneShotTimer sync_service_startup_timeout_timer_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  base::WeakPtrFactory<SyncServiceStartupStateObserverImpl>
      weak_pointer_factory_{this};
};

SyncServiceStartupStateObserverImpl::SyncServiceStartupStateObserverImpl(
    syncer::SyncService* sync_service,
    base::TimeDelta startup_delay,
    base::OnceClosure on_state_updated_callback)
    : on_state_updated_callback_(std::move(on_state_updated_callback)) {
  CHECK(sync_service);
  CHECK(on_state_updated_callback_);
  // Start a timeout for sync service to update its state.
  sync_service_startup_timeout_timer_.Start(
      FROM_HERE, startup_delay,
      base::BindOnce(
          &SyncServiceStartupStateObserverImpl::OnSyncServiceStartupTimeout,
          weak_pointer_factory_.GetWeakPtr()));
  sync_service_observation_.Observe(sync_service);
}

SyncServiceStartupStateObserverImpl::~SyncServiceStartupStateObserverImpl() =
    default;

// static
std::unique_ptr<SyncServiceStartupStateObserver>
SyncServiceStartupStateObserverImpl::
    MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
        syncer::SyncService* sync_service,
        Profile* profile,
        const CoreAccountInfo& account_info,
        base::TimeDelta startup_delay,
        base::OnceClosure callback) {
  if (AccountMayHaveCloudPolicies(profile, account_info.email) &&
      IsSyncStartupInPendingState(sync_service)) {
    // The service is still starting up, wait for it to become active or
    // disabled.
    return std::make_unique<SyncServiceStartupStateObserverImpl>(
        sync_service, startup_delay, std::move(callback));
  }
  return nullptr;
}

void SyncServiceStartupStateObserverImpl::MockTimeoutReachedForTesting() {
  sync_service_startup_timeout_timer_.FireNow();
}

void SyncServiceStartupStateObserverImpl::
    OnSyncStartupStateChangedForTesting(  // IN-TEST
        SyncStartupTracker::ServiceStartupState state) {
  // Tests using this implementation should set the transport state of the sync
  // service directly.
  NOTREACHED();
}

void SyncServiceStartupStateObserverImpl::OnStateChanged(
    syncer::SyncService* sync) {
  if (!IsSyncStartupInPendingState(sync)) {
    // The sync service has finished starting up, so we can stop observing.
    if (sync_startup_complete_metrics_callback_) {
      std::move(sync_startup_complete_metrics_callback_)
          .Run(GetElapsedTime(sync_service_startup_timeout_timer_));
    }
    sync_service_startup_timeout_timer_.Stop();
    sync_service_observation_.Reset();
    std::move(on_state_updated_callback_).Run();
  }
}

void SyncServiceStartupStateObserverImpl::OnSyncShutdown(
    syncer::SyncService* sync) {
  sync_service_startup_timeout_timer_.Stop();
  sync_service_observation_.Reset();
}

void SyncServiceStartupStateObserverImpl::OnSyncServiceStartupTimeout() {
  if (timeout_metrics_callback_) {
    std::move(timeout_metrics_callback_)
        .Run(sync_service_startup_timeout_timer_.GetCurrentDelay());
  }
  sync_service_startup_timeout_timer_.Stop();
  sync_service_observation_.Reset();
  CHECK(!on_state_updated_callback_.is_null());
  std::move(on_state_updated_callback_).Run();
}

// An implementation of `SyncServiceStartupStateObserver` based on the
// soon to be deprecated `SyncStartupTracker`.

// TurnSyncOnHelper is deprecated. `HistorySyncOptinHelper` should use the
// `SyncServiceStartupStateObserver` (currently gated by a feature flag).
class SyncServiceStartupStateLegacyObserverImpl
    : public SyncServiceStartupStateObserver {
 public:
  SyncServiceStartupStateLegacyObserverImpl(
      syncer::SyncService* sync_service,
      base::OnceClosure on_state_updated_callback);
  ~SyncServiceStartupStateLegacyObserverImpl() override;

  static std::unique_ptr<SyncServiceStartupStateObserver>
  MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
      syncer::SyncService* sync_service,
      Profile* profile,
      const CoreAccountInfo& account_info,
      base::OnceClosure callback);
  // SyncServiceStartupStateObserver implementation:
  void MockTimeoutReachedForTesting() override;  // IN-TEST
  void OnSyncStartupStateChangedForTesting(
      SyncStartupTracker::ServiceStartupState state) override;  // IN-TEST

 private:
  void OnSyncStartupStateChanged(SyncStartupTracker::ServiceStartupState state);

  base::OnceClosure on_state_updated_callback_;
  std::unique_ptr<SyncStartupTracker> sync_startup_tracker_;
  base::WeakPtrFactory<SyncServiceStartupStateLegacyObserverImpl>
      weak_pointer_factory_{this};
};

SyncServiceStartupStateLegacyObserverImpl::
    SyncServiceStartupStateLegacyObserverImpl(
        syncer::SyncService* sync_service,
        base::OnceClosure on_state_updated_callback)
    : on_state_updated_callback_(std::move(on_state_updated_callback)) {
  CHECK(sync_service);
  CHECK(on_state_updated_callback_);

  sync_startup_tracker_ = std::make_unique<SyncStartupTracker>(
      sync_service,
      base::BindOnce(
          &SyncServiceStartupStateLegacyObserverImpl::OnSyncStartupStateChanged,
          weak_pointer_factory_.GetWeakPtr()));
  return;
}

SyncServiceStartupStateLegacyObserverImpl::
    ~SyncServiceStartupStateLegacyObserverImpl() = default;

// static
std::unique_ptr<SyncServiceStartupStateObserver>
SyncServiceStartupStateLegacyObserverImpl::
    MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
        syncer::SyncService* sync_service,
        Profile* profile,
        const CoreAccountInfo& account_info,
        base::OnceClosure callback) {
  if (AccountMayHaveCloudPolicies(profile, account_info.email) &&
      SyncStartupTracker::GetServiceStartupState(sync_service) ==
          SyncStartupTracker::ServiceStartupState::kPending) {
    return std::make_unique<SyncServiceStartupStateLegacyObserverImpl>(
        sync_service, std::move(callback));
  }
  return nullptr;
}

void SyncServiceStartupStateLegacyObserverImpl::OnSyncStartupStateChanged(
    SyncStartupTracker::ServiceStartupState state) {
  switch (state) {
    case SyncStartupTracker::ServiceStartupState::kPending:
      NOTREACHED();
    case SyncStartupTracker::ServiceStartupState::kTimeout:
      [[fallthrough]];
    case SyncStartupTracker::ServiceStartupState::kError:
    case SyncStartupTracker::ServiceStartupState::kComplete:
      std::move(on_state_updated_callback_).Run();
  }
}

void SyncServiceStartupStateLegacyObserverImpl::MockTimeoutReachedForTesting() {
  // Tests using this implementation can make use of
  // `testing::ScopedSyncStartupTimeoutOverride`.
  NOTREACHED();
}

void SyncServiceStartupStateLegacyObserverImpl::
    OnSyncStartupStateChangedForTesting(
        SyncStartupTracker::ServiceStartupState state) {
  OnSyncStartupStateChanged(state);
}
}  // namespace

BASE_FEATURE(kEnableAwaitSyncServiceStartupOnHistorySync,
             base::FEATURE_ENABLED_BY_DEFAULT);

SyncServiceStartupStateObserver::SyncServiceStartupStateObserver() = default;
SyncServiceStartupStateObserver::~SyncServiceStartupStateObserver() = default;

// static
std::unique_ptr<SyncServiceStartupStateObserver>
SyncServiceStartupStateObserver::
    MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
        syncer::SyncService* sync_service,
        Profile* profile,
        const CoreAccountInfo& account_info,
        base::TimeDelta startup_delay,
        base::OnceClosure callback) {
  if (base::FeatureList::IsEnabled(
          kEnableAwaitSyncServiceStartupOnHistorySync) &&
      syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    return SyncServiceStartupStateObserverImpl::
        MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
            sync_service, profile, account_info, startup_delay,
            std::move(callback));
  }
  return SyncServiceStartupStateLegacyObserverImpl::
      MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
          sync_service, profile, account_info, std::move(callback));
}

void SyncServiceStartupStateObserver::SetSyncStartupCompleteMetricsCallback(
    base::OnceCallback<void(base::TimeDelta)> callback) {
  CHECK(callback);
  sync_startup_complete_metrics_callback_ = std::move(callback);
}

void SyncServiceStartupStateObserver::SetTimeoutMetricsCallback(
    base::OnceCallback<void(base::TimeDelta)> callback) {
  CHECK(callback);
  timeout_metrics_callback_ = std::move(callback);
}
