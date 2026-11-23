// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/test/browser_test.h"

namespace unified_consent {
namespace {

class UnifiedConsentBrowserTest : public SyncTest {
 public:
  UnifiedConsentBrowserTest() : SyncTest(TWO_CLIENT) {}

  UnifiedConsentBrowserTest(const UnifiedConsentBrowserTest&) = delete;
  UnifiedConsentBrowserTest& operator=(const UnifiedConsentBrowserTest&) =
      delete;

  ~UnifiedConsentBrowserTest() override = default;

  void EnableSync(int client_id) {
    InitializeSyncClientsIfNeeded();

    ASSERT_TRUE(GetClient(client_id)->SetupSync());
  }

  UnifiedConsentService* consent_service() {
    return UnifiedConsentServiceFactory::GetForProfile(browser()->GetProfile());
  }

 protected:
  base::HistogramTester histogram_tester_;
  const std::string histogram_name_ =
      "UnifiedConsent.MakeSearchesAndBrowsingBetter.OnProfileLoad";

 private:
  void InitializeSyncClientsIfNeeded() {
    if (GetSyncClients().empty())
      ASSERT_TRUE(SetupClients());
  }
};

// Tests that the settings histogram is recorded if unified consent is enabled.
// The histogram is recorded during profile initialization.
IN_PROC_BROWSER_TEST_F(UnifiedConsentBrowserTest, SettingsHistogram_None) {
  histogram_tester_.ExpectUniqueSample(histogram_name_, false, 1);
}

// Tests that all service entries in the settings histogram are recorded after
// enabling them.
IN_PROC_BROWSER_TEST_F(
    UnifiedConsentBrowserTest,
    PRE_SettingsHistogram_UrlKeyedAnonymizedDataCollectionEnabled) {
  EnableSync(0);
  consent_service()->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentBrowserTest,
    SettingsHistogram_UrlKeyedAnonymizedDataCollectionEnabled) {
  histogram_tester_.ExpectUniqueSample(histogram_name_, true, 1);
}

}  // namespace
}  // namespace unified_consent
