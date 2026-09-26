// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/metrics/testing/metrics_consent_override.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/dwa/dwa_entry_builder.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/dwa/dwa_service.h"
#include "components/metrics/private_metrics/private_metrics_features.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/federated_compute/src/fcp/confidentialcompute/cose.h"
#include "third_party/federated_compute/src/fcp/confidentialcompute/crypto.h"
#include "third_party/federated_compute/src/fcp/confidentialcompute/crypto_test_util.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace metrics::dwa {

namespace {

std::string CreatePublicKeyForTesting() {
  auto public_key =
      fcp::confidential_compute::GenerateHpkeKeyPair("key-id").first;
  auto decoded = fcp::confidential_compute::OkpCwt::Decode(public_key);

  // DWA validates the existence of the algorithm field, set it to 0 for tests.
  decoded->algorithm = 0;

  return decoded->Encode().value();
}

}  // namespace

using PlatformBrowser = BrowserWindowInterface*;

DwaService* GetDwaService() {
  return g_browser_process->GetMetricsServicesManager()->GetDwaService();
}

bool IsDwaAllowedForAllProfiles() {
  return g_browser_process->GetMetricsServicesManager()
      ->IsDwaAllowedForAllProfiles();
}

// Test fixture that provides access to some DWA internals.
class DwaBrowserTest : public SyncTest {
 public:
  DwaBrowserTest() : SyncTest(SINGLE_CLIENT) {
    // Explicitly enable DWA and disable metrics reporting. Disabling metrics
    // reporting should affect only UMA--not DWA.
    scoped_feature_list_.InitWithFeatures(
        {dwa::kDwaFeature, private_metrics::kPrivateMetricsFeature},
        {internal::kMetricsReportingFeature});
  }

  DwaBrowserTest(const DwaBrowserTest&) = delete;
  DwaBrowserTest& operator=(const DwaBrowserTest&) = delete;

  void AssertDwaIsEnabledAndAllowed() const {
    ASSERT_TRUE(metrics::dwa::DwaRecorder::Get()->IsEnabled());
    ASSERT_TRUE(IsDwaAllowedForAllProfiles());
  }

  void AssertDwaRecorderHasMetrics() const {
    ASSERT_TRUE(metrics::dwa::DwaRecorder::Get()->HasEntries());
  }

  void ExpectDwaIsDisabledAndDisallowed() const {
    EXPECT_FALSE(metrics::dwa::DwaRecorder::Get()->IsEnabled());
    EXPECT_FALSE(IsDwaAllowedForAllProfiles());
  }

  void ExpectDwaIsEnabledAndAllowed() const {
    EXPECT_TRUE(metrics::dwa::DwaRecorder::Get()->IsEnabled());
    EXPECT_TRUE(IsDwaAllowedForAllProfiles());
  }

  void ExpectDwaRecorderIsEmpty() const {
    EXPECT_FALSE(metrics::dwa::DwaRecorder::Get()->HasEntries());
  }

  void RecordTestDwaEntryMetric() {
    ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
    builder.SetContent("https://adtech.com");
    builder.SetMetric("Length", 5);
    builder.Record(dwa::DwaRecorder::Get());
  }

  void RecordTestMetricsAndAssertMetricsRecorded() {
    RecordTestDwaEntryMetric();
    AssertDwaRecorderHasMetrics();
  }

  void SetupDwaService() {
    auto public_key = CreatePublicKeyForTesting();
    GetDwaService()->SetEncryptionPublicKeyForTesting(public_key);
    GetDwaService()->SetEncryptionPublicKeyVerifierForTesting(
        base::BindRepeating([](const fcp::confidential_compute::OkpCwt&)
                                -> bool { return true; }));
  }

  void SetMsbbConsentState(Profile* profile, bool consent_state) {
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    ASSERT_NE(consent_service, nullptr);

    if (consent_service) {
      consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(
          consent_state);
    }
  }

  void SetExtensionsConsentState(Profile* profile, bool consent_state) {
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    ASSERT_NE(consent_service, nullptr);

    std::unique_ptr<SyncServiceImplHarness> harness =
        SyncServiceImplHarness::Create(
            profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
    EXPECT_TRUE(harness->SetupSync());

    if (consent_state) {
      ASSERT_TRUE(harness->EnableSelectableType(
          syncer::UserSelectableType::kExtensions));
    } else {
      ASSERT_TRUE(harness->DisableSelectableType(
          syncer::UserSelectableType::kExtensions));
    }
  }

  void SetAppsConsentState(Profile* profile, bool consent_state) {
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    ASSERT_NE(consent_service, nullptr);

    std::unique_ptr<SyncServiceImplHarness> harness =
        SyncServiceImplHarness::Create(
            profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
    EXPECT_TRUE(harness->SetupSync());

    if (consent_state) {
      ASSERT_TRUE(
          harness->EnableSelectableType(syncer::UserSelectableType::kApps));
    } else {
      ASSERT_TRUE(
          harness->DisableSelectableType(syncer::UserSelectableType::kApps));
    }
  }

 protected:
  std::unique_ptr<SyncServiceImplHarness> EnableSyncForProfile(
      Profile* profile) {
    std::unique_ptr<SyncServiceImplHarness> harness =
        SyncServiceImplHarness::Create(
            profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
    EXPECT_TRUE(harness->SetupSync());

    // If unified consent is enabled, then enable url-keyed-anonymized data
    // collection through the consent service.
    // Note: If unified consent is not enabled, then DWA will be enabled based
    // on the history sync state.
    SetMsbbConsentState(profile, true);

    return harness;
  }

  // Creates and returns a platform-appropriate browser for |profile|.
  PlatformBrowser CreatePlatformBrowser(Profile* profile) {
    return CreateBrowser(profile);
  }

  // Creates a platform-appropriate incognito browser for |profile|.
  PlatformBrowser CreateIncognitoPlatformBrowser(Profile* profile) {
    EXPECT_TRUE(profile->IsOffTheRecord());
    return CreateIncognitoBrowser(profile);
  }

  // Closes |browser| in a way that is appropriate for the platform.
  void ClosePlatformBrowser(PlatformBrowser& browser) {
    CloseBrowserSynchronously(browser);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// LINT.IfChange(DwaServiceCheck)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, DwaServiceCheck) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);
  SetupDwaService();

  dwa::DwaService* dwa_service = GetDwaService();
  dwa::DwaRecorder* dwa_recorder = metrics::dwa::DwaRecorder::Get();

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  ASSERT_TRUE(dwa_recorder->IsEnabled());

  // Records a DWA entry metric.
  RecordTestDwaEntryMetric();
  EXPECT_TRUE(dwa_recorder->HasEntries());
  EXPECT_FALSE(dwa_service->unsent_log_store()->has_unsent_logs());

  GetDwaService()->Flush(
      metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  ExpectDwaRecorderIsEmpty();
  EXPECT_TRUE(dwa_service->unsent_log_store()->has_unsent_logs());

  ClosePlatformBrowser(browser);
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/dwa_egtest.mm:DwaServiceCheck)

// Make sure that DWA is disabled and purged while an incognito window is open.
// LINT.IfChange(RegularBrowserPlusIncognitoCheck)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, RegularBrowserPlusIncognitoCheck) {
  dwa::DwaRecorder* dwa_recorder = metrics::dwa::DwaRecorder::Get();
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  // DWA should be enabled and capable of recording metrics when opening the
  // first regular browser.
  PlatformBrowser browser1 = CreatePlatformBrowser(profile);
  ASSERT_TRUE(dwa_recorder->IsEnabled());
  RecordTestDwaEntryMetric();
  EXPECT_TRUE(dwa_recorder->HasEntries());

  // Opening an incognito browser should disable DwaRecorder and metrics should
  // be purged.
  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  PlatformBrowser incognito_browser1 =
      CreateIncognitoPlatformBrowser(incognito_profile);
  ASSERT_FALSE(dwa_recorder->IsEnabled());
  EXPECT_FALSE(dwa_recorder->HasEntries());
  RecordTestDwaEntryMetric();
  EXPECT_FALSE(dwa_recorder->HasEntries());

  // Opening another regular browser should not enable DWA.
  PlatformBrowser browser2 = CreatePlatformBrowser(profile);
  ASSERT_FALSE(dwa_recorder->IsEnabled());
  RecordTestDwaEntryMetric();
  EXPECT_FALSE(dwa_recorder->HasEntries());

  // Opening and closing another Incognito browser must not enable DWA.
  PlatformBrowser incognito_browser2 =
      CreateIncognitoPlatformBrowser(incognito_profile);
  ClosePlatformBrowser(incognito_browser2);
  ASSERT_FALSE(dwa_recorder->IsEnabled());
  RecordTestDwaEntryMetric();
  EXPECT_FALSE(dwa_recorder->HasEntries());

  ClosePlatformBrowser(browser2);
  ASSERT_FALSE(dwa_recorder->IsEnabled());
  RecordTestDwaEntryMetric();
  EXPECT_FALSE(dwa_recorder->HasEntries());

  // Closing all incognito browsers should enable DwaRecorder and we should be
  // able to log metrics again.
  ClosePlatformBrowser(incognito_browser1);
  ASSERT_TRUE(dwa_recorder->IsEnabled());
  EXPECT_FALSE(dwa_recorder->HasEntries());
  RecordTestDwaEntryMetric();
  EXPECT_TRUE(dwa_recorder->HasEntries());

  ClosePlatformBrowser(browser1);
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/dwa_egtest.mm:RegularBrowserPlusIncognitoCheck)

// Make sure opening a regular browser after Incognito doesn't enable DWA.
// LINT.IfChange(IncognitoPlusRegularBrowserCheck)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, IncognitoPlusRegularBrowserCheck) {
  dwa::DwaRecorder* dwa_recorder = metrics::dwa::DwaRecorder::Get();
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  PlatformBrowser incognito_browser =
      CreateIncognitoPlatformBrowser(incognito_profile);
  ASSERT_FALSE(dwa_recorder->IsEnabled());

  PlatformBrowser browser = CreatePlatformBrowser(profile);
  ASSERT_FALSE(dwa_recorder->IsEnabled());

  ClosePlatformBrowser(incognito_browser);
  ASSERT_TRUE(dwa_recorder->IsEnabled());

  ClosePlatformBrowser(browser);
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/dwa_egtest.mm:IncognitoPlusRegularBrowserCheck)

// This test ensures that disabling MSBB UKM consent disables and purges DWA.
// Additionally ensures that DWA is disabled until all UKM consents are enabled.
// LINT.IfChange(UkmMsbbConsentChangeCheck)
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, UkmConsentChangeCheck_Msbb) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off MSBB consent.
  SetMsbbConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on MSBB consent.
  SetMsbbConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}
// LINT.ThenChange(/ios/chrome/browser/metrics/model/dwa_egtest.mm:UkmMsbbConsentChangeCheck)

// This test ensures that disabling Extensions UKM consent disables and purges
// DWA. Additionally ensures that DWA is disabled until all UKM consents are
// enabled.
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, UkmConsentChangeCheck_Extensions) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off Extension consent.
  SetExtensionsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on Extension consent.
  SetExtensionsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}

// This test ensures that disabling Apps UKM consent disables and purges DWA.
// Additionally ensures that DWA is disabled until all UKM consents are enabled.
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, UkmConsentChangeCheck_Apps) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off Apps consent.
  SetAppsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on Apps consent.
  SetAppsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}

// This test ensures that disabling MSBB and Extensions UKM consents disables
// and purges DWA. Additionally ensures that DWA is disabled until all UKM
// consents are enabled.
IN_PROC_BROWSER_TEST_F(DwaBrowserTest,
                       UkmConsentChangeCheck_MsbbAndExtensions) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off MSBB and Extension consent.
  SetMsbbConsentState(profile, /*consent_state=*/false);
  SetExtensionsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on MSBB should not enable DWA because Extensions consent is still
  // disabled.
  SetMsbbConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on Extensions consent.
  SetExtensionsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}

// This test ensures that disabling MSBB and Apps UKM consents disables and
// purges DWA. Additionally ensures that DWA is disabled until all UKM consents
// are enabled.
IN_PROC_BROWSER_TEST_F(DwaBrowserTest, UkmConsentChangeCheck_MsbbAndApps) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off MSBB and Apps consent.
  SetMsbbConsentState(profile, /*consent_state=*/false);
  SetAppsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on MSBB should not enable DWA because Apps consent is still
  // disabled.
  SetMsbbConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on Apps consent.
  SetAppsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}

// This test ensures that disabling Extensions and Apps UKM consents disables
// and purges DWA. Additionally ensures that DWA is disabled until all UKM
// consents are enabled.
IN_PROC_BROWSER_TEST_F(DwaBrowserTest,
                       UkmConsentChangeCheck_ExtensionsAndApps) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off Extensions and Apps consent.
  SetExtensionsConsentState(profile, /*consent_state=*/false);
  SetAppsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on Extensions should not enable DWA because Apps consent is still
  // disabled.
  SetExtensionsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turn on Apps consent.
  SetAppsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}

// This test ensures that disabling MSBB, Extensions, and Apps UKM consents
// disables and purges DWA. Additionally ensures that DWA is disabled until all
// UKM consents are enabled.
IN_PROC_BROWSER_TEST_F(DwaBrowserTest,
                       UkmConsentChangeCheck_MsbbAndExtensionsAndApps) {
  test::MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  EnableSyncForProfile(profile);

  RecordTestDwaEntryMetric();
  AssertDwaIsEnabledAndAllowed();
  AssertDwaRecorderHasMetrics();

  // Turn off MSBB, Apps, and Extensions consent.
  SetMsbbConsentState(profile, /*consent_state=*/false);
  SetExtensionsConsentState(profile, /*consent_state=*/false);
  SetAppsConsentState(profile, /*consent_state=*/false);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on Apps consent should not enable DWA because MSBB and Extensions
  // consent are still disabled.
  SetAppsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on Extensions should not enable DWA because MSBB is still disabled.
  SetExtensionsConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsDisabledAndDisallowed();
  ExpectDwaRecorderIsEmpty();
  // Turning on MSBB consent should enable DWA.
  SetMsbbConsentState(profile, /*consent_state=*/true);
  ExpectDwaIsEnabledAndAllowed();
  ExpectDwaRecorderIsEmpty();

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  RecordTestMetricsAndAssertMetricsRecorded();
}

}  // namespace metrics::dwa
