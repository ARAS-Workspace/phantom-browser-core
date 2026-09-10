// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/ukm_consent_state_observer.h"

#include "base/observer_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_profile_pref_names.h"
#include "components/metrics/metrics_reporting_choice_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/sync_token_status.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ukm {

namespace {

class MockSyncService : public syncer::TestSyncService {
 public:
  MockSyncService() {
    SetMaxTransportState(TransportState::INITIALIZING);
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot());

  }

  MockSyncService(const MockSyncService&) = delete;
  MockSyncService& operator=(const MockSyncService&) = delete;

  ~MockSyncService() override { Shutdown(); }

  void SetStatus(bool has_passphrase, bool history_enabled, bool active) {
    SetMaxTransportState(active ? TransportState::ACTIVE
                                : TransportState::INITIALIZING);
    SetIsUsingExplicitPassphrase(has_passphrase);

    GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/history_enabled ? syncer::UserSelectableTypeSet(
                                        {syncer::UserSelectableType::kHistory})
                                  : syncer::UserSelectableTypeSet());

    // It doesn't matter what exactly we set here, it's only relevant that the
    // SyncCycleSnapshot is initialized at all.
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot(
        /*birthday=*/std::string(), /*bag_of_chips=*/std::string(),
        syncer::ModelNeutralState(), syncer::ProgressMarkerMap(), false, 0,
        true, base::Time::Now(), base::Time::Now(),
        sync_pb::SyncEnums::UNKNOWN_ORIGIN, base::Minutes(1), false));

    NotifyObserversOfStateChanged();
  }

  void Shutdown() override {
    for (auto& observer : observers_) {
      observer.OnSyncShutdown(this);
    }
  }

 private:
  // syncer::TestSyncService:
  void AddObserver(syncer::SyncServiceObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(syncer::SyncServiceObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyObserversOfStateChanged() {
    for (auto& observer : observers_) {
      observer.OnStateChanged(this);
    }
  }

  // The list of observers of the SyncService state.
  base::ObserverList<syncer::SyncServiceObserver> observers_;
};

class TestUkmConsentStateObserver : public UkmConsentStateObserver {
 public:
  // Inherits UkmConsentStateObserver constructors.
  using UkmConsentStateObserver::UkmConsentStateObserver;

  TestUkmConsentStateObserver(const TestUkmConsentStateObserver&) = delete;
  TestUkmConsentStateObserver& operator=(const TestUkmConsentStateObserver&) =
      delete;

  ~TestUkmConsentStateObserver() override = default;

  bool ResetPurged() {
    bool was_purged = purged_;
    purged_ = false;
    return was_purged;
  }

  bool ResetNotified() {
    bool notified = notified_;
    notified_ = false;
    return notified;
  }

 private:
  // UkmConsentStateObserver:
  void OnUkmAllowedStateChanged(bool must_purge, UkmConsentState) override {
    notified_ = true;
    purged_ = purged_ || must_purge;
  }
  bool purged_ = false;
  bool notified_ = false;
};

class UkmConsentStateObserverTest : public testing::TestWithParam<bool> {
 public:
  UkmConsentStateObserverTest() = default;

  UkmConsentStateObserverTest(const UkmConsentStateObserverTest&) = delete;
  UkmConsentStateObserverTest& operator=(const UkmConsentStateObserverTest&) =
      delete;

  void RegisterUrlKeyedAnonymizedDataCollectionPref(
      sync_preferences::TestingPrefServiceSyncable& prefs) {
    unified_consent::UnifiedConsentService::RegisterPrefs(prefs.registry());
    metrics::MetricsReportingChoiceService::RegisterProfilePrefs(
        prefs.registry());
  }

  void SetUrlKeyedAnonymizedDataCollectionEnabled(PrefService* prefs,
                                                  bool enabled) {
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        enabled);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

void ExpectDwaAllowedForAllProfiles(TestUkmConsentStateObserver& observer,
                                    bool expected_allowed) {
  // App sync is turned off by default in CHROMEOS. This results in DWA not
  // being allowed for that particular test setup, however can be enabled for
  // other platforms. DWA are tested further below.
  EXPECT_EQ(expected_allowed, observer.IsDwaAllowedForAllProfiles());
}

}  // namespace

TEST_F(UkmConsentStateObserverTest, NoProfiles) {
  TestUkmConsentStateObserver observer;
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, false);
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, NotActive) {
  MockSyncService sync;
  sync.SetStatus(false, true, false);
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestUkmConsentStateObserver observer;
  observer.StartObserving(&sync, &prefs);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, false);
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, OneEnabled) {
  MockSyncService sync;
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  TestUkmConsentStateObserver observer;
  observer.StartObserving(&sync, &prefs);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, true);
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, MixedProfiles) {
  sync_preferences::TestingPrefServiceSyncable prefs1;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs1);
  sync_preferences::TestingPrefServiceSyncable prefs2;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs2);

  TestUkmConsentStateObserver observer;
  MockSyncService sync1;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs1, false);
  observer.StartObserving(&sync1, &prefs1);
  MockSyncService sync2;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs2, true);
  observer.StartObserving(&sync2, &prefs2);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, false);
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, TwoEnabled) {
  sync_preferences::TestingPrefServiceSyncable prefs1;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs1);
  sync_preferences::TestingPrefServiceSyncable prefs2;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs2);
  TestUkmConsentStateObserver observer;
  MockSyncService sync1;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs1, true);
  observer.StartObserving(&sync1, &prefs1);
  EXPECT_TRUE(observer.ResetNotified());
  MockSyncService sync2;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs2, true);
  observer.StartObserving(&sync2, &prefs2);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, true);
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, OneAddRemove) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestUkmConsentStateObserver observer;
  MockSyncService sync;

  observer.StartObserving(&sync, &prefs);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, false);
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, true);
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, false);
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, PurgeOnDisable) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestUkmConsentStateObserver observer;
  MockSyncService sync;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  observer.StartObserving(&sync, &prefs);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, true);
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, false);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, false);
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_TRUE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, false);
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, NoInitialUkmConsentState) {
  MockSyncService sync;
  sync.SetStatus(false, true, false);
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestUkmConsentStateObserver observer(NoInitialUkmConsentState);
  observer.StartObserving(&sync, &prefs);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  ExpectDwaAllowedForAllProfiles(observer, false);
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  sync.Shutdown();
}

class UkmConsentStateObserverMigrationTest
    : public UkmConsentStateObserverTest {
 public:
  UkmConsentStateObserverMigrationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        metrics::features::kRestructureMetricsConsentSettings);
  }
};

TEST_F(UkmConsentStateObserverMigrationTest, Migration_SignedOut_MSBBEnabled) {
  base::HistogramTester histogram_tester;
  MockSyncService sync;
  sync.SetSignedOut();

  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);

  TestUkmConsentStateObserver observer;
  observer.StartObserving(&sync, &prefs);

  EXPECT_TRUE(prefs.GetBoolean(metrics::prefs::kAdvancedReportingEnabled));
  EXPECT_TRUE(
      prefs.GetBoolean(metrics::prefs::kAdvancedReportingProfileMigrationDone));

  histogram_tester.ExpectUniqueSample(
      "UKM.ConsentObserver.ConsentStateBeforeRestructure",
      /*sample=*/2, 1);  // kMsbbEnabledWithAppAndExtensionSync
}

TEST_F(UkmConsentStateObserverMigrationTest, Migration_SignedIn_AllEnabled) {
  base::HistogramTester histogram_tester;
  MockSyncService sync;
  sync.SetSignedIn(signin::ConsentLevel::kSignin);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Mark apps and extensions sync as supported (registered) and enabled.
  syncer::UserSelectableTypeSet registered_types = {
      syncer::UserSelectableType::kExtensions};
  syncer::UserSelectableTypeSet selected_types = {
      syncer::UserSelectableType::kExtensions};
  registered_types.Put(syncer::UserSelectableType::kApps);
  selected_types.Put(syncer::UserSelectableType::kApps);
  sync.GetUserSettings()->SetRegisteredSelectableTypes(registered_types);
  sync.GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                           selected_types);
#endif

  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);

  TestUkmConsentStateObserver observer;
  observer.StartObserving(&sync, &prefs);

  EXPECT_TRUE(prefs.GetBoolean(metrics::prefs::kAdvancedReportingEnabled));
  EXPECT_TRUE(
      prefs.GetBoolean(metrics::prefs::kAdvancedReportingProfileMigrationDone));

  histogram_tester.ExpectUniqueSample(
      "UKM.ConsentObserver.ConsentStateBeforeRestructure",
      /*sample=*/2, 1);  // kMsbbEnabledWithAppAndExtensionSync
}

TEST_F(UkmConsentStateObserverMigrationTest, Migration_SignedIn_APPSDisabled) {
  base::HistogramTester histogram_tester;
  MockSyncService sync;
  sync.SetSignedIn(signin::ConsentLevel::kSignin);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Mark apps and extensions sync as supported, but only extensions enabled.
  syncer::UserSelectableTypeSet registered_types = {
      syncer::UserSelectableType::kExtensions};
  registered_types.Put(syncer::UserSelectableType::kApps);
  sync.GetUserSettings()->SetRegisteredSelectableTypes(registered_types);
  sync.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kExtensions});
#endif

  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);

  TestUkmConsentStateObserver observer;
  observer.StartObserving(&sync, &prefs);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // On mobile platforms, apps sync is not supported, so it is ignored. MSBB is
  // enabled, so migration succeeds!
  EXPECT_TRUE(prefs.GetBoolean(metrics::prefs::kAdvancedReportingEnabled));
  histogram_tester.ExpectUniqueSample(
      "UKM.ConsentObserver.ConsentStateBeforeRestructure",
      /*sample=*/2, 1);  // kMsbbEnabledWithAppAndExtensionSync
#else
  // On desktop platforms, apps sync is supported but disabled, so migration
  // fails!
  EXPECT_FALSE(prefs.GetBoolean(metrics::prefs::kAdvancedReportingEnabled));
  histogram_tester.ExpectUniqueSample(
      "UKM.ConsentObserver.ConsentStateBeforeRestructure",
      /*sample=*/1, 1);  // kMsbbEnabledWithoutAppOrExtensionSync
#endif
  EXPECT_TRUE(
      prefs.GetBoolean(metrics::prefs::kAdvancedReportingProfileMigrationDone));
}

TEST_F(UkmConsentStateObserverMigrationTest, Migration_AlreadyDone) {
  base::HistogramTester histogram_tester;
  MockSyncService sync;
  sync.SetSignedIn(signin::ConsentLevel::kSignin);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  syncer::UserSelectableTypeSet registered_types = {
      syncer::UserSelectableType::kExtensions};
  syncer::UserSelectableTypeSet selected_types = {
      syncer::UserSelectableType::kExtensions};
  registered_types.Put(syncer::UserSelectableType::kApps);
  selected_types.Put(syncer::UserSelectableType::kApps);
  sync.GetUserSettings()->SetRegisteredSelectableTypes(registered_types);
  sync.GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                           selected_types);
#endif

  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);

  // Set post-migration states where user disabled advanced reporting.
  prefs.SetBoolean(metrics::prefs::kAdvancedReportingEnabled, false);
  prefs.SetBoolean(metrics::prefs::kAdvancedReportingProfileMigrationDone,
                   true);

  TestUkmConsentStateObserver observer;
  observer.StartObserving(&sync, &prefs);

  // Migration should not re-run.
  EXPECT_FALSE(prefs.GetBoolean(metrics::prefs::kAdvancedReportingEnabled));
  EXPECT_TRUE(
      prefs.GetBoolean(metrics::prefs::kAdvancedReportingProfileMigrationDone));

  histogram_tester.ExpectUniqueSample(
      "UKM.ConsentObserver.ConsentStateBeforeRestructure",
      /*sample=*/2, 1);  // kMsbbEnabledWithAppAndExtensionSync
}

}  // namespace ukm
