// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the chrome
// module.

#ifndef CHROME_COMMON_CHROME_FEATURES_H_
#define CHROME_COMMON_CHROME_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "pdf/buildflags.h"
#include "ui/base/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAppShimRemoteCocoa);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppShimNewCloseBehavior);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppShimLaunchChromeSilently);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppShimNotificationAttribution);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUseAdHocSigningForWebAppShims);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUseKeychainKeyProvider);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillAddressSurvey);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillCardSurvey);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillPasswordSurvey);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAndroidKeepProfilePartitionDirsInCacheDir);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kBoardingPassDetector);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kBoardingPassDetectorUrlParam;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kBoardingPassDetectorUrlParamName[];
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCaptureHandleForStandalonePwasAndIwas);
#endif  // !BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCryptographyComplianceCnsa);

// Delays BEST_EFFORT tasks during startup until tabs are loaded/idle and first
// paint.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kImprovedStartupBestEffortDelay);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kStartupDelayFailsafeTimeout);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kStartupDelayVisibleTabTimeout);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE_PARAM(bool, kStartupDelayStopOnLoadingTimedOut);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE_PARAM(bool, kStartupDelayIncludesSessionRestore);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPreinstalledWebAppInstallation);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPreinstalledWebAppAlwaysMigrateForTesting);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kChromeStructuredMetrics);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDeferSpellcheckInitialization);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsElidedExtensionsMenu);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsPreventClose);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsTabStripSettings);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsWindowControlsOverlayWithNoToggle);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDocumentPipStandaloneWindow);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kRemoteActorCredentialSharing);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kRemoteActorCredentialSharingAllowedHostForTesting;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
bool RemoteActorCredentialSharingEnabled();

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kChromeAppsDeprecation);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDisplayEdgeToEdgeFullscreen);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableFullscreenToAnyScreenAndroid);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicActor);

BASE_DECLARE_FEATURE(kGlicBackgroundTriggering);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicActorUi);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kActorUiThemed);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kGlicActorUiToastName[];

// Exempts the user from ActorPolicyChecker.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicActorPolicyControlExemption;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicCountryFiltering);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicLocaleFiltering);

// Controls whether the Glic feature is enabled.
// IMPORTANT: this feature should never be expired! It is used as the main
// kill-switch for Glic and can be used in the future to handle unsupported
// Chrome versions.
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlic);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicBackgroundActuation);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicRollout);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicDefaultTabContextSetting);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGoogleChromeScheme);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGoogleSearchAiModeWorkspace);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrivacyGuideForceAvailable);

COMPONENT_EXPORT(CHROME_FEATURES)

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopDemo);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysConfiguration);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kHappinessTrackingSurveysHostedUrl;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopSettings);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopSettingsTime;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopSettingsPrivacy);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyNoGuide;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyTime;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopPrivacyGuide);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopPrivacyGuideTime;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopNtpModules);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopNextPanel);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopHistoryPageExperiment);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopHistoryPageExperimentTime;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopHistoryPageControl);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopHistoryPageControlTime;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForHistoryEmbeddings);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForHistoryEmbeddingsDelayTime;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForNtpPhotosOptOut);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForWallpaperSearch);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopWhatsNew);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopWhatsNewTime;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHaTSDesktopDevToolsIssuesCOEP);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHaTSDesktopDevToolsIssuesMixedContent);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(
    kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHaTSDesktopDevToolsIssuesHeavyAd);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHaTSDesktopDevToolsIssuesCSP);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopSEHijacking);

#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHttpsFirstBalancedMode);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstBalancedModeAutoEnable);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeForAdvancedProtectionUsers);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeDefaultSettingPairsWithEsb);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeV2ForEngagedSites);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeV2ForTypicallySecureUsers);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHttpsUpgrades);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsUpgradesTypedSchemelessNavigationNoTimeoutFallback);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kHttpsUpgradesFallbackDelay;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHttpsUpgradesAskBeforeHttpFallbackDelay;
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeIncognito);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIncomingCallNotifications);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kInitialWebUIWithoutSpellCheckForNtp);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kInstantUsesSpareRenderer);
#endif  // !BUILDFLAG(IS_ANDROID)

// LINT.IfChange
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kIsolatedWebAppDevMode);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIsolatedWebAppUnmanagedInstall);
// LINT.ThenChange(//PRESUBMIT.py)

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kIsolatedWebAppDevUi);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIsolatedWebAppFastUpdateCheck);

#if BUILDFLAG(IS_LINUX)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kLinuxLowMemoryMonitor);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kLinuxLowMemoryMonitorModerateLevel;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kLinuxLowMemoryMonitorCriticalLevel;
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kListWebAppsSwitch);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kLazyKeyedServiceInstantiation);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE_PARAM(bool,
                           kLazyKeyedServiceInstantiationAutofillAndPassword);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kNativeNotifications);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSystemNotifications);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kNoReferrers);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kOfflineAutoFetch);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kOnConnectNative);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kOomIntervention);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrerenderFallbackToPreconnect);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPwaUpdateDialogForIcon);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kQuietNotificationPrompts);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kRecordWebAppDebugInfo);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAbusiveNotificationPermissionRevocation);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsUwSTrigger);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsNoPrivacyPracticesTrigger);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsOffStoreTrigger);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubThreeDotDetails);

// Automatically revoke disruptive notifications
// in Safety Hub.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubDisruptiveNotificationRevocation);

// And integer which tracks the current version of the running experiment for
// disruptive notification revocation. Proposed revocations will be versioned
// and ignored upon version change. This allows to ignore proposed revocations
// from previous experiments in order to consistently revoke and report metrics.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationExperimentVersion;

// Whether the disruptive notification revocation will be performed as a shadow
// run (without actually revoking permissions). Used to collect metrics and
// evaluate the conditions for autorevocation.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>
    kSafetyHubDisruptiveNotificationRevocationShadowRun;

// The minimum number of average daily notifications over last 7 days for a
// website to classify for disruptive notification revocation. Used in a
// combination with
// `kSafetyHubDisruptiveNotificationRevocationMaxEngagementScore`.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMinNotificationCount;

// The maximum site engagement score for a website to classify for disruptive
// notification revocation. Used in a combination with
// `kSafetyHubDisruptiveNotificationRevocationMinNotificationCount`.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kSafetyHubDisruptiveNotificationRevocationMaxEngagementScore;

// The waiting time for a website classified as sending disruptive notifications
// before notification permission is revoked. The website has to satisfy the
// disruptive requirements for this amount of time before the revocation is
// actually enforced.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kSafetyHubDisruptiveNotificationRevocationWaitingTimeAsProposed;

// Timeout in seconds for the Safety Hub OS notification informing users about
// revoked notification permissions.
#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationNotificationTimeoutSeconds;
#endif

// The minimum number of days since the revocation until a site can be
// considered a false positive disruptive notification revocation. The cooldown
// period allows to gather interactions for a period of time to understand how
// much users have interacted with a site and whether it might have been a flake
// (ex. accidental click on a notification).
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMinFalsePositiveCooldown;

// The maximum number of days since the revocation when a site can be considered
// a false positive disruptive notification revocation. After it runs out, the
// revocation won't be reported as a false positive.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMaxFalsePositivePeriod;

// The minimum site engagement score delta for a website to be considered a
// false positive disruptive notification revocation.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kSafetyHubDisruptiveNotificationRevocationMinSiteEngagementScoreDelta;

// The maximum number of days to observe the revoked site for user regranting
// the permission while visiting the site. The period is a number of days since
// a false positive was detected (a page visit or a notification click).
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationUserRegrantWaitingPeriod;

// The maximum number of days to wait for metrics to be reported for proposed
// disruptive notification revocation. After the period runs out, the permission
// will be revoked. The number is a number of days since a revocation was
// proposed.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationWaitingForMetricsDays;

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubWeakAndReusedPasswords);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubLocalPasswordsModule);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubUnifiedPasswordsModule);
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubTrustSafetySentimentSurvey);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSCTAuditingHashdance);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kSCTLogExpectedIngestionDelay;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kSCTLogMaxIngestionRandomDelay;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kServiceWorkerForegroundOnExtensionStartup);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kExtensionServiceWorkerPriorityVoter);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSitePerProcess);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kProcessPerSiteSkipDevtoolsUsers);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kProcessPerSiteSkipEnterpriseUsers);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kProcessPerSiteForDSE);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kConsiderDSEWarmUpPageAsSRP);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kTreatUnsafeDownloadsAsActive);

// TrustSafetySentimentSurvey
#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kTrustSafetySentimentSurvey);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyMinTimeToPrompt;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyMaxTimeToPrompt;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kTrustSafetySentimentSurveyNtpVisitsMinRange;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kTrustSafetySentimentSurveyNtpVisitsMaxRange;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySettingsProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyTrustedSurfaceProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyTransactionsProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySettingsTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTrustedSurfaceTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTransactionsTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyPrivacySettingsTime;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyTrustedSurfaceTime;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyTransactionsPasswordManagerTime;
#endif

// TrustSafetySentimentSurveyV2
#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kTrustSafetySentimentSurveyV2);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MinTimeToPrompt;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MaxTimeToPrompt;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kTrustSafetySentimentSurveyV2NtpVisitsMinRange;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kTrustSafetySentimentSurveyV2NtpVisitsMaxRange;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MinSessionTime;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2BrowsingDataProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2ControlGroupProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2DownloadWarningUIProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PasswordCheckProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PasswordProtectionUIProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyCheckProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyHubNotificationProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyHubInteractionProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2TrustedSurfaceProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacyGuideProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2BrowsingDataTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2ControlGroupTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2DownloadWarningUITriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PasswordCheckTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PasswordProtectionUITriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyCheckTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyHubInteractionTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyHubNotificationTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2TrustedSurfaceTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacyGuideTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2TrustedSurfaceTime;
#endif

#if !BUILDFLAG(IS_ANDROID)
// Periodically query a preinstalled app for updating, with the intention of
// doing this for all preinstalled apps with cheap install_urls (that do not
// redirect etc).
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppPeriodicPreinstallUpdate);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppMigratePreinstalledChat);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppInstallDialog);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppInstallDialogWinPin);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppHandleAppMigrationViaSync);
#endif  // !BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppUpgradeToDatabaseVersion6);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebium);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kInitialWebUIMetrics);
// TODO(crbug.com/444358999): after the experiment to collect metrics, either
// remove this reload button web UI or extend it to include more top-chrome
// components.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIReloadButton);
// When `kWebUIToolbarProcessOverheadExperiment` is enabled, the
// WebUIToolbar will always be initialized and never added into the view
// hierarchy. i.e. it overrides other WebUIToolbar checks.
// TODO(crbug.com/503575910): remove this flag and all the related checking
// logic once we have collected enough data from the experiment.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIToolbarProcessOverheadExperiment);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kWebUIReloadButtonMaxCrashRecoveryTimes;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kWebUIReloadButtonCrashRecoverResetInterval;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kWebUIReloadButtonCrashRecoverRetryInterval;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kWebUIReloadButtonRestartUnresponsive;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kWebUIReloadButtonRestartUnresponsiveRenderersTimeout;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kWebUIReloadButtonDeferBrowserViewShow;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kWebUIReloadButtonPrewarmWebUI;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kWebUIReloadButtonPrewarmWebUIPreNavigate;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kWebUIReloadButtonProfilePrewarming;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kWebUIReloadButtonKeepVisibleUntilPaint;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kWebUIReloadButtonBypassLoaderThrottles;
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIHomeButton);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIBatterySaverButton);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIPerformanceInterventionButton);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIAppMenuButton);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIBackForwardButton);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIPinnedToolbarActions);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIExtensionsContainer);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUISplitTabsButton);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUILocationBar);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIToolbar);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIToolbarFrameEvictionOptOut);
#endif  // !BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kRespectUserAgentOverrideInSearchPrefetch);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kRestrictedWebUICodeCache);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kRestrictedWebUICodeCacheResources;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUserValueDefaultBrowserStrings);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSmartRestartMetrics);

// Enables "Smart Restarts", allowing Chrome to silently apply updates when
// the browser is in a zero-disruption state (e.g., zero windows on macOS).
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSmartRestart);

// The amount of time to wait after entering a potential restart state (like
// zero windows) before executing the restart.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kSmartRestartDelay;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSmartRestartLockScreen);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kSmartRestartLockScreenTabThreshold;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kSmartRestartLockScreenDisruptionThreshold;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kSmartRestartLockScreenDelay;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kSmartRestartLockBypassBeforeUnloadThreshold;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kRecordTabWindowDiffOnRestart);
#endif  // BUILDFLAG(IS_ANDROID)

bool PrefServiceEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CHROME_COMMON_CHROME_FEATURES_H_
