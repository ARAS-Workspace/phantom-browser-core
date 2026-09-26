// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/features.h"

#include "base/feature_list.h"

namespace syncer {

BASE_FEATURE(kDeferredSyncStartupCustomDelay,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncSharedTabGroupAccountData, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncSharedComment, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAIThread, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncContextualTask, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncGeminiThread, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEncryptedTabContextContainer,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncThemesIos, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewTabPageCustomizationThemeSync,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncLoyaltyCardMetadata, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncNotebook, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncJourney, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUnoPhase2FollowUp, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsContactInfoDataTypeForCustomPassphraseUsersEnabled() {
  return base::FeatureList::IsEnabled(
             kSyncEnableContactInfoDataTypeForCustomPassphraseUsers) ||
         base::FeatureList::IsEnabled(
             kReplaceSyncPromosWithSigninPromosNewSignin);
}

BASE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateLocalAndAccountSearchEngines,
             base::FEATURE_ENABLED_BY_DEFAULT
);

BASE_FEATURE(kReplaceSyncPromosWithSignInPromos,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReplaceSyncPromosWithSigninPromosNewSignin,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsReplaceSyncPromosWithSignInPromosEnabled() {
  return base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos) ||
         base::FeatureList::IsEnabled(
             kReplaceSyncPromosWithSigninPromosNewSignin);
}

// Like DECLARE_SYNC_AUTOFILL_AI_FEATURE but for the definition.
#define DEFINE_SYNC_AUTOFILL_AI_FEATURE(feature_name) \
  BASE_FEATURE(feature_name, base::FEATURE_ENABLED_BY_DEFAULT)

DEFINE_SYNC_AUTOFILL_AI_FEATURE(kSyncAccountSettings);

DEFINE_SYNC_AUTOFILL_AI_FEATURE(kSyncAutofillValuableMetadata);

DEFINE_SYNC_AUTOFILL_AI_FEATURE(kSyncWalletFlightReservations);

DEFINE_SYNC_AUTOFILL_AI_FEATURE(kSyncWalletVehicleRegistrations);

#undef DEFINE_SYNC_AUTOFILL_AI_FEATURE

BASE_FEATURE(kSpellcheckSeparateLocalAndAccountDictionaries,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn,
             base::FEATURE_ENABLED_BY_DEFAULT
);

bool IsReadingListAccountStorageEnabled() {
  return base::FeatureList::IsEnabled(
      syncer::kReadingListEnableSyncTransportModeUponSignIn);
}

// Enabled by default, intended as a kill switch.
BASE_FEATURE(kSyncReadingListBatchUploadSelectedItems,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateLocalAndAccountThemes,
             base::FEATURE_ENABLED_BY_DEFAULT
);

BASE_FEATURE(kSyncIncreaseNudgeDelayForSingleClient,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncPreferencesUseSelectedTypes,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableNewSyncDashboardUrl, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncRecordDeviceStatisticsMetrics,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSyncRecordDeviceStatisticsMetricsDelay,
                   &kSyncRecordDeviceStatisticsMetrics,
                   "SyncRecordDeviceStatisticsMetricsDelay",
                   base::Seconds(30));
BASE_FEATURE_PARAM(int,
                   kSyncRecordDeviceStatisticsMetricsPeriodDays,
                   &kSyncRecordDeviceStatisticsMetrics,
                   "SyncRecordDeviceStatisticsMetricsPeriodDays",
                   1);

BASE_FEATURE(kSyncValidateAccessToken, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncUsePropagatedAccessToken, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncInvalidationsBypassScheduler,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncFixWebSigninSessionDurationForShortLivedSessions,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncSimplifyDeviceNaming, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncDisambiguateDeviceNamesWithChannel,
             "SyncDisambiguateDeviceNamesWithChannel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncUseServerDeterminedDeviceName,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncCopyPreferencesToTransportModeOnServerForcedDisable,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncNigoriAuthenticateIV, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace syncer
