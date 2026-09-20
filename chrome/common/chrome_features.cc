// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "build/android_buildflags.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "pdf/buildflags.h"

namespace features {

// All features in alphabetical order.

#if BUILDFLAG(IS_MAC)
// Can be used to disable RemoteCocoa (hosting NSWindows for apps in the app
// process). For debugging purposes only.
BASE_FEATURE(kAppShimRemoteCocoa, base::FEATURE_ENABLED_BY_DEFAULT);

// This is used to control the new app close behavior on macOS wherein closing
// all windows for an app leaves the app running.
// https://crbug.com/40130206
BASE_FEATURE(kAppShimNewCloseBehavior, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, app shims try to launch chrome silently if chrome isn't already
// running, rather than have chrome launch visibly with a new tab/profile
// selector.
// https://crbug.com/40180521
BASE_FEATURE(kAppShimLaunchChromeSilently, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, notifications coming from PWAs will be displayed via their app
// shim processes, rather than directly by chrome.
// https://crbug.com/40616749
BASE_FEATURE(kAppShimNotificationAttribution,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, app shims used by PWAs will be signed with an ad-hoc signature
// https://crbug.com/40276068
BASE_FEATURE(kUseAdHocSigningForWebAppShims, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the KeychainKeyProvider is used to provide the OS Crypt async
// key.
BASE_FEATURE(kUseKeychainKeyProvider, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables or disables the Autofill survey triggered by opening a prompt to
// save address info.
BASE_FEATURE(kAutofillAddressSurvey, base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Autofill survey triggered by opening a prompt to
// save credit card info.
BASE_FEATURE(kAutofillCardSurvey, base::FEATURE_DISABLED_BY_DEFAULT);
// Enables or disables the Autofill survey triggered by opening a prompt to
// save password info.
BASE_FEATURE(kAutofillPasswordSurvey, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// When enabled, GetUserCacheDirectory on Android will append the relative path
// of non-default partitions to the cache directory.
BASE_FEATURE(kAndroidKeepProfilePartitionDirsInCacheDir,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable boarding pass detector on Chrome Android.
BASE_FEATURE(kBoardingPassDetector, base::FEATURE_DISABLED_BY_DEFAULT);
const char kBoardingPassDetectorUrlParamName[] = "boarding_pass_detector_urls";
const base::FeatureParam<std::string> kBoardingPassDetectorUrlParam(
    &kBoardingPassDetector,
    kBoardingPassDetectorUrlParamName,
    "");
#endif  // BUILDFLAG(IS_ANDROID)


#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kCaptureHandleForStandalonePwasAndIwas,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)


// Enables stricter cryptography settings for CNSA2 compliance. This is not
// needed for security, but may be required by some organizations.
BASE_FEATURE(kCryptographyComplianceCnsa, base::FEATURE_DISABLED_BY_DEFAULT);

// Delays BEST_EFFORT tasks during startup until tabs are loaded/idle and first
// paint.
BASE_FEATURE(kImprovedStartupBestEffortDelay,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Sets the timeout until startup is declared "finished" even if not all
// StartupInProgressRefs have been dropped.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kStartupDelayFailsafeTimeout,
                   &kImprovedStartupBestEffortDelay,
                   base::Minutes(3));

// Sets the timeout until the startup observer stops waiting for a visible tab.
// This can happen if a dialog is shown on start (eg. the profile picker), or in
// Mac's zero-window mode. If a tab appears before this timeout, the observer
// waits for it to fully load. If this is 0, the startup observer won't wait for
// tabs to become loaded/idle.
//
// The default matches kWaitingForNavigationTimeout because before the
// kImprovedStartupBestEffortDelay feature, many startups were marked "finished"
// at that point.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kStartupDelayVisibleTabTimeout,
                   &kImprovedStartupBestEffortDelay,
                   base::Seconds(5));

// If true, the startup observer will consider a tab "finished" if it reaches
// the kLoadingTimedOut state, instead of just kLoadedIdle.
BASE_FEATURE_PARAM(bool,
                   kStartupDelayStopOnLoadingTimedOut,
                   &kImprovedStartupBestEffortDelay,
                   false);

// If true, session restore will create a StartupInProgressRef, and drop it when
// restore is finished.
BASE_FEATURE_PARAM(bool,
                   kStartupDelayIncludesSessionRestore,
                   &kImprovedStartupBestEffortDelay,
                   true);

#if !BUILDFLAG(IS_ANDROID)
// Whether to allow installed-by-default web apps to be installed or not.
BASE_FEATURE(kPreinstalledWebAppInstallation,
             "DefaultWebAppInstallation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to force migrate preinstalled web apps whenever the old Chrome app
// they're replacing is detected, even if the web app is already installed.
// Used by unit tests.
BASE_FEATURE(kPreinstalledWebAppAlwaysMigrateForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kRemoteActorCredentialSharing, base::FEATURE_DISABLED_BY_DEFAULT);
// This parameter is for testing purposes only and must not be used in
// production. It overrides the whitelisted origins with the specified host.
const base::FeatureParam<std::string>
    kRemoteActorCredentialSharingAllowedHostForTesting{
        &kRemoteActorCredentialSharing, "allowed_host_for_testing", ""};
#endif

bool RemoteActorCredentialSharingEnabled() {
#if !BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(features::kRemoteActorCredentialSharing);
#else
  return false;
#endif
}

// Controls the enablement of structured metrics on Windows, Linux, and Mac.
BASE_FEATURE(kChromeStructuredMetrics, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeferSpellcheckInitialization, base::FEATURE_DISABLED_BY_DEFAULT);

// Moves the Extensions "puzzle piece" icon from the title bar into the app menu
// for web app windows.
BASE_FEATURE(kDesktopPWAsElidedExtensionsMenu,
             base::FEATURE_DISABLED_BY_DEFAULT
);

// If enabled, allow-listed PWAs cannot be closed manually by the user.
BASE_FEATURE(kDesktopPWAsPreventClose,
             base::FEATURE_DISABLED_BY_DEFAULT
);

// Adds a user settings that allows PWAs to be opened with a tab strip.
BASE_FEATURE(kDesktopPWAsTabStripSettings, base::FEATURE_DISABLED_BY_DEFAULT);

// Removes the Window Controls Overlay toggle button from the PWA toolbar
// and auto-enables WCO when the app developer specifies it in the manifest.
// When disabled, the toggle button is shown and WCO must be enabled by the
// user. This feature only affects apps that have "window-controls-overlay" in
// their PWA manifest's display_override field, apps without manifest support
// are unaffected.
BASE_FEATURE(kDesktopPWAsWindowControlsOverlayWithNoToggle,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the standalone Document Picture-in-Picture window path, replacing
// the Browser-backed implementation with a dedicated host.
BASE_FEATURE(kDocumentPipStandaloneWindow, base::FEATURE_DISABLED_BY_DEFAULT);

// Allows fullscreen to claim whole display area when in windowing mode
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kDisplayEdgeToEdgeFullscreen, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables Fullscreen to Screen on Android platform
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableFullscreenToAnyScreenAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Controls whether Chrome Apps are supported. See https://crbug.com/40186761.
// If the feature is disabled, Chrome Apps continue to work. If enabled, Chrome
// Apps will not launch and will be marked in the UI as deprecated.
BASE_FEATURE(kChromeAppsDeprecation, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)


// Controls whether the actor component of Glic is enabled.
BASE_FEATURE(kGlicActor, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicBackgroundTriggering, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the Actor UI components are enabled.
BASE_FEATURE(kGlicActorUi, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls theming updates for Actor UI, including the tab indicator spinner
// and other elements.
BASE_FEATURE(kActorUiThemed, base::FEATURE_ENABLED_BY_DEFAULT);

const char kGlicActorUiToastName[] = "glic-actor-ui-toast";

const base::FeatureParam<bool> kGlicActorPolicyControlExemption{
    &kGlicActor, "glic_actor_policy_control_exemption", false};

// Controls country and locale filtering for Glic.
BASE_FEATURE(kGlicCountryFiltering, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGlicLocaleFiltering, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the Glic feature is enabled.
// IMPORTANT: this feature should never be expired! It is used as the main
// kill-switch for Glic and can be used in the future to handle unsupported
// Chrome versions.
BASE_FEATURE(kGlic,
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kGlicBackgroundActuation, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicRollout, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicDefaultTabContextSetting, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
#else
#endif

// Whether to enable OneTimePassword filling in Glic.
// TODO(b/500683394): Clean up after launch.
BASE_FEATURE(kGlicActorAutofillOneTimePassword,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the `google-chrome://` URI scheme.
BASE_FEATURE(kGoogleChromeScheme, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the Google Search AI Mode Workspace link (Connected Apps) is
// shown in AI Settings. Acts as a killswitch.
BASE_FEATURE(kGoogleSearchAiModeWorkspace, base::FEATURE_ENABLED_BY_DEFAULT);

// Force Privacy Guide to be available even if it would be unavailable
// otherwise. This is meant for development and test purposes only.
BASE_FEATURE(kPrivacyGuideForceAvailable, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_PDF)
#endif

#if !BUILDFLAG(IS_ANDROID)
// Enables or disables the Happiness Tracking System demo mode for Desktop
// Chrome.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopDemo,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHappinessTrackingSurveysConfiguration,
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kHappinessTrackingSurveysHostedUrl{
    &kHappinessTrackingSurveysConfiguration, "custom-url",
    "https://www.google.com/chrome/hats/index_m129.html"};

// Enables or disables the Happiness Tracking System for COEP issues in Chrome
// DevTools on Desktop.
BASE_FEATURE(kHaTSDesktopDevToolsIssuesCOEP, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Mixed Content issues in
// Chrome DevTools on Desktop.
BASE_FEATURE(kHaTSDesktopDevToolsIssuesMixedContent,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for same-site cookies
// issues in Chrome DevTools on Desktop.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Heavy Ad issues in
// Chrome DevTools on Desktop.
BASE_FEATURE(kHaTSDesktopDevToolsIssuesHeavyAd,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for CSP issues in Chrome
// DevTools on Desktop.
BASE_FEATURE(kHaTSDesktopDevToolsIssuesCSP, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Desktop Privacy Guide.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopPrivacyGuide,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopPrivacyGuideTime{
        &kHappinessTrackingSurveysForDesktopPrivacyGuide, "settings-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for Desktop History Page in
// the Experiment group.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopHistoryPageExperiment,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopHistoryPageExperimentTime{
        &kHappinessTrackingSurveysForDesktopHistoryPageExperiment,
        "history-page-time", base::Seconds(5)};

// Enables or disables the Happiness Tracking System for Desktop History Page in
// the Control group.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopHistoryPageControl,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopHistoryPageControlTime{
        &kHappinessTrackingSurveysForDesktopHistoryPageControl,
        "history-page-time", base::Seconds(5)};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// Settings.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopSettings,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopSettingsTime{
        &kHappinessTrackingSurveysForDesktopSettings, "settings-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// Privacy Settings.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopSettingsPrivacy,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyNoGuide{
        &kHappinessTrackingSurveysForDesktopSettingsPrivacy, "no-guide", false};
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyTime{
        &kHappinessTrackingSurveysForDesktopSettingsPrivacy, "settings-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for Desktop Chrome
// NTP Modules.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopNtpModules,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Desktop Chrome
// Next Panel.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopNextPanel,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for History Embeddings.
BASE_FEATURE(kHappinessTrackingSurveysForHistoryEmbeddings,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForHistoryEmbeddingsDelayTime(
        &kHappinessTrackingSurveysForHistoryEmbeddings,
        "HappinessTrackingSurveysForHistoryEmbeddingsDelayTime",
        base::Seconds(20));

BASE_FEATURE(kHappinessTrackingSurveysForNtpPhotosOptOut,
             "HappinessTrackingSurveysForrNtpPhotosOptOut",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Wallpaper Search.
BASE_FEATURE(kHappinessTrackingSurveysForWallpaperSearch,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the Happiness Tracking System for Chrome What's New.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopWhatsNew,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopWhatsNewTime{
        &kHappinessTrackingSurveysForDesktopWhatsNew, "whats-new-time",
        base::Seconds(20)};

// Enables or disables the Happiness Tracking System for SE Hijacking.
BASE_FEATURE(kHappinessTrackingSurveysForDesktopSEHijacking,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

// Enables HTTPS-First Mode in a balanced configuration that doesn't warn on
// HTTP when HTTPS can't be reasonably expected.
BASE_FEATURE(kHttpsFirstBalancedMode, base::FEATURE_ENABLED_BY_DEFAULT);

// Automatically enables HTTPS-First Mode in a balanced configuration when
// possible.
BASE_FEATURE(kHttpsFirstBalancedModeAutoEnable,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for crbug.com/40892208.
BASE_FEATURE(kHttpsFirstModeForAdvancedProtectionUsers,
             "HttpsOnlyModeForAdvancedProtectionUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHttpsFirstModeDefaultSettingPairsWithEsb,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables HTTPS-First Mode for engaged sites. No-op if HttpsFirstModeV2 or
// HTTPS-Upgrades is disabled.
BASE_FEATURE(kHttpsFirstModeV2ForEngagedSites,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables HTTPS-First Mode for typically secure users. No-op if
// HttpsFirstModeV2 or HTTPS-Upgrades is disabled.
BASE_FEATURE(kHttpsFirstModeV2ForTypicallySecureUsers,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables automatically upgrading main frame navigations to HTTPS.
BASE_FEATURE(kHttpsUpgrades, base::FEATURE_ENABLED_BY_DEFAULT);
// When enabled, typed schemeless navigations (e.g., typed "example.com" in the
// Omnibox) that are upgraded to HTTPS will not fallback to HTTP if the HTTPS
// navigation fails due to a timeout.
BASE_FEATURE(kHttpsUpgradesTypedSchemelessNavigationNoTimeoutFallback,
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kHttpsUpgradesFallbackDelay{
    &kHttpsUpgrades, "fallback-delay", base::Seconds(3)};

const base::FeatureParam<base::TimeDelta>
    kHttpsUpgradesAskBeforeHttpFallbackDelay{
        &kHttpsUpgrades, "ask-before-http-fallback-delay", base::Seconds(5)};

// Enables HTTPS-First Mode by default in Incognito Mode.
BASE_FEATURE(kHttpsFirstModeIncognito, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Incoming Call Notifications scenario. When created by an
// installed origin, an incoming call notification should have increased
// priority, colored buttons, a ringtone, and a default "close" button.
// Otherwise, if the origin is not installed, it should behave like the default
// notifications, but with the added "Close" button. See
// https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/Notifications/notifications_actions_customization.md
BASE_FEATURE(kIncomingCallNotifications,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
// A feature that controls whether Instant uses a spare renderer.
BASE_FEATURE(kInstantUsesSpareRenderer, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

// Enables Isolated Web App Developer Mode, which allows developers to
// install untrusted Isolated Web Apps.
BASE_FEATURE(kIsolatedWebAppDevMode, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the chrome://iwa-dev WebUI page.
BASE_FEATURE(kIsolatedWebAppDevUi, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables fast update checks for Isolated Web Apps, reducing the update check
// interval to 1 minute.
BASE_FEATURE(kIsolatedWebAppFastUpdateCheck, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables users on unmanaged devices to install Isolated Web Apps.
BASE_FEATURE(kIsolatedWebAppUnmanagedInstall,
             base::FEATURE_DISABLED_BY_DEFAULT
);

#if BUILDFLAG(IS_LINUX)
BASE_FEATURE(kLinuxLowMemoryMonitor, base::FEATURE_DISABLED_BY_DEFAULT);
// Values taken from the low-memory-monitor documentation and also apply to the
// portal API:
// https://hadess.pages.freedesktop.org/low-memory-monitor/gdbus-org.freedesktop.LowMemoryMonitor.html
constexpr base::FeatureParam<int> kLinuxLowMemoryMonitorModerateLevel{
    &kLinuxLowMemoryMonitor, "moderate_level", 50};
constexpr base::FeatureParam<int> kLinuxLowMemoryMonitorCriticalLevel{
    &kLinuxLowMemoryMonitor, "critical_level", 255};
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
BASE_FEATURE(kListWebAppsSwitch, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, keyed services are instantiated lazily rather than eagerly at
// startup.
BASE_FEATURE(kLazyKeyedServiceInstantiation, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, autofill and password manager keyed services are instantiated
// lazily.
BASE_FEATURE_PARAM(bool,
                   kLazyKeyedServiceInstantiationAutofillAndPassword,
                   &features::kLazyKeyedServiceInstantiation,
                   "autofill_and_password",
                   true);

// Enables the use of system notification centers instead of using the Message
// Center for displaying the toasts. The feature is hardcoded to enabled for
// Chrome OS.
BASE_FEATURE(kNativeNotifications, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the initial WebUI skips spell check initialization on startup for
// NTP.
BASE_FEATURE(kInitialWebUIWithoutSpellCheckForNtp,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSystemNotifications, base::FEATURE_ENABLED_BY_DEFAULT);

// When kNoReferrers is enabled, most HTTP requests will provide empty
// referrers instead of their ordinary behavior.
BASE_FEATURE(kNoReferrers, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kOfflineAutoFetch, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
BASE_FEATURE(kOnConnectNative, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// Enables or disables the OOM intervention.
BASE_FEATURE(kOomIntervention, base::FEATURE_ENABLED_BY_DEFAULT);
#endif


// Allows Chrome to do preconnect when prerender fails.
BASE_FEATURE(kPrerenderFallbackToPreconnect, base::FEATURE_DISABLED_BY_DEFAULT);


BASE_FEATURE(kUserValueDefaultBrowserStrings,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Shows a confirmation dialog when updates to a PWAs icon has been detected.
BASE_FEATURE(kPwaUpdateDialogForIcon, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using quiet prompts for notification permission requests.
BASE_FEATURE(kQuietNotificationPrompts, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables recording additional web app related debugging data to be displayed
// in: chrome://web-app-internals
BASE_FEATURE(kRecordWebAppDebugInfo, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables notification permission revocation for abusive origins.
BASE_FEATURE(kAbusiveNotificationPermissionRevocation,
             "AbusiveOriginNotificationPermissionRevocation",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
BASE_FEATURE(kSafetyHubExtensionsUwSTrigger, base::FEATURE_ENABLED_BY_DEFAULT);
// Enables extensions that do not display proper privacy practices in the
// Safety Hub Extension Reivew Panel.
BASE_FEATURE(kSafetyHubExtensionsNoPrivacyPracticesTrigger,
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables offstore extensions to be shown in the Safety Hub Extension
// review panel.
BASE_FEATURE(kSafetyHubExtensionsOffStoreTrigger,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSafetyHubThreeDotDetails, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyHubDisruptiveNotificationRevocation,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool>
    kSafetyHubDisruptiveNotificationRevocationShadowRun{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"shadow_run", /*default_value=*/false};

#if BUILDFLAG(IS_ANDROID)
constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationExperimentVersion{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"experiment_version", /*default_value=*/1};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMinNotificationCount{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"min_notification_count", /*default_value=*/4};

constexpr base::FeatureParam<base::TimeDelta>
    kSafetyHubDisruptiveNotificationRevocationWaitingTimeAsProposed{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"waiting_time_as_proposed", /*default_value=*/base::Days(4)};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationNotificationTimeoutSeconds{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"notification_timeout_seconds",
        /*default_value=*/7 * 24 * 3600};
#else
constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationExperimentVersion{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"experiment_version", /*default_value=*/2};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMinNotificationCount{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"min_notification_count", /*default_value=*/6};

constexpr base::FeatureParam<base::TimeDelta>
    kSafetyHubDisruptiveNotificationRevocationWaitingTimeAsProposed{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"waiting_time_as_proposed", /*default_value=*/base::Days(14)};
#endif

constexpr base::FeatureParam<double>
    kSafetyHubDisruptiveNotificationRevocationMaxEngagementScore{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"max_engagement_score", /*default_value=*/0.0};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMinFalsePositiveCooldown{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"min_false_positive_cooldown", /*default_value=*/0};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMaxFalsePositivePeriod{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"max_false_positive_period", /*default_value=*/14};

// TODO(crbug.com/406472515): Site engagement score increase on navigation
// happens at the same time as us detecting the navigation. If the score delta
// is 0, the initial navigation won't trigger marking the site as false
// positive.
constexpr base::FeatureParam<double>
    kSafetyHubDisruptiveNotificationRevocationMinSiteEngagementScoreDelta{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"min_engagement_score_delta", /*default_value=*/3.0};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationUserRegrantWaitingPeriod{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"user_regrant_waiting_period", /*default_value=*/7};

constexpr base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationWaitingForMetricsDays{
        &kSafetyHubDisruptiveNotificationRevocation,
        /*name=*/"waiting_for_metrics_days", /*default_value=*/1};

#if BUILDFLAG(IS_ANDROID)
// Enables Weak and Reused passwords in Safety Hub.
BASE_FEATURE(kSafetyHubWeakAndReusedPasswords,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the local passwords module in Safety Hub.
BASE_FEATURE(kSafetyHubLocalPasswordsModule, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the unified passwords module in Safety Hub.
BASE_FEATURE(kSafetyHubUnifiedPasswordsModule,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Enables or disables the Trust Safety Sentiment Survey for Safety Hub.
BASE_FEATURE(kSafetyHubTrustSafetySentimentSurvey,
             "TrustSafetySentimentSurveyForSafetyHub",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

// SCT auditing hashdance allows Chrome clients who are not opted-in to Enhanced
// Safe Browsing Reporting to perform a k-anonymous query to see if Google knows
// about an SCT seen in the wild. If it hasn't been seen, then it is considered
// a security incident and uploaded to Google.
BASE_FEATURE(kSCTAuditingHashdance, base::FEATURE_ENABLED_BY_DEFAULT);

// An estimated high bound for the time it takes Google to ingest updates to an
// SCT log. Chrome will wait for at least this time plus the Log's Maximum Merge
// Delay after an SCT's timestamp before performing a hashdance lookup query.
const base::FeatureParam<base::TimeDelta> kSCTLogExpectedIngestionDelay{
    &kSCTAuditingHashdance,
    "sct_log_expected_ingestion_delay",
    base::Hours(1),
};

// A random delay will be added to the expected log ingestion delay between zero
// and this maximum. This prevents a burst of queries once a new SCT is issued.
const base::FeatureParam<base::TimeDelta> kSCTLogMaxIngestionRandomDelay{
    &kSCTAuditingHashdance,
    "sct_log_max_ingestion_random_delay",
    base::Hours(1),
};

// When enabled, an extension service worker's render process is given
// foreground priority while the worker is STARTING. Extension service workers
// are often started headlessly (e.g. to register webRequest listeners) with no
// controllee or other foreground signal, so their process would otherwise be
// left at background priority (which maps to EcoQoS on Windows). Under heavy
// system load that lets the worker starve, miss the start timeout, get torn
// down, and retry indefinitely (crbug.com/484218883). The boost is dropped once
// the worker reaches RUNNING or stops.
BASE_FEATURE(kServiceWorkerForegroundOnExtensionStartup,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, a performance manager voter keeps the renderer process of an
// extension service worker at foreground priority for as long as the worker
// lives, but only when the extension holds the `webRequestBlocking` permission.
// Those workers synchronously gate navigations and network requests, so letting
// their process drop to background priority (EcoQoS on Windows) stalls the
// browsing session (crbug.com/484218883). This is the narrowly scoped
// counterpart to performance_manager::features::kExtensionServiceWorkerVoter,
// which boosts every extension service worker and regressed performance metrics
// (crbug.com/493556675).
BASE_FEATURE(kExtensionServiceWorkerPriorityVoter,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Alternative to switches::kSitePerProcess, for turning on full site isolation.
// Launch bug: https://crbug.com/810843.  This is a //chrome-layer feature to
// avoid turning on site-per-process by default for *all* //content embedders
// (e.g. this approach lets ChromeCast avoid site-per-process mode).
//
// TODO(alexmos): Move this and the other site isolation features below to
// browser_features, as they are only used on the browser side.
BASE_FEATURE(kSitePerProcess,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// The default behavior to opt devtools users out of
// kProcessPerSiteUpToMainFrameThreshold.
BASE_FEATURE(kProcessPerSiteSkipDevtoolsUsers,
             base::FEATURE_DISABLED_BY_DEFAULT);

// The default behavior to opt enterprise users out of
// kProcessPerSiteUpToMainFrameThreshold.
BASE_FEATURE(kProcessPerSiteSkipEnterpriseUsers,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Restricts "ProcessPerSiteUpToMainFrameThreshold" to the default search
// engine. Has no effect if "ProcessPerSiteUpToMainFrameThreshold" is disabled.
// Note: The "ProcessPerSiteUpToMainFrameThreshold" feature is defined in
// //content.

BASE_FEATURE(kProcessPerSiteForDSE,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Consider the default search engine (DSE) warmup page as a search results page
// (SRP), for the purpose of applying the "process per site for DSE SRP" policy
// (`kProcessPerSiteForDSE`).
BASE_FEATURE(kConsiderDSEWarmUpPageAsSRP, base::FEATURE_ENABLED_BY_DEFAULT);


// Disable downloads of unsafe file types over insecure transports if initiated
// from a secure page. As of M89, mixed downloads are blocked on all platforms.
BASE_FEATURE(kTreatUnsafeDownloadsAsActive, base::FEATURE_ENABLED_BY_DEFAULT);

// TrustSafetySentimentSurvey
#if !BUILDFLAG(IS_ANDROID)
// Enables surveying of users of Trust & Safety features with HaTS.
BASE_FEATURE(kTrustSafetySentimentSurvey, base::FEATURE_DISABLED_BY_DEFAULT);
// The minimum and maximum time after a user has interacted with a Trust and
// Safety they are eligible to be surveyed.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyMinTimeToPrompt{
        &kTrustSafetySentimentSurvey, "min-time-to-prompt", base::Minutes(2)};
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyMaxTimeToPrompt{
        &kTrustSafetySentimentSurvey, "max-time-to-prompt", base::Minutes(60)};
// The maximum and minimum range for the random number of NTPs that the user
// must at least visit after interacting with a Trust and Safety feature to be
// eligible for a survey.
const base::FeatureParam<int> kTrustSafetySentimentSurveyNtpVisitsMinRange{
    &kTrustSafetySentimentSurvey, "ntp-visits-min-range", 2};
const base::FeatureParam<int> kTrustSafetySentimentSurveyNtpVisitsMaxRange{
    &kTrustSafetySentimentSurvey, "ntp-visits-max-range", 4};
// The feature area probabilities for each feature area considered as part of
// the Trust & Safety sentiment survey.
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySettingsProbability{
        &kTrustSafetySentimentSurvey, "privacy-settings-probability", 0.6};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyTrustedSurfaceProbability{
        &kTrustSafetySentimentSurvey, "trusted-surface-probability", 0.4};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyTransactionsProbability{
        &kTrustSafetySentimentSurvey, "transactions-probability", 0.05};
// The HaTS trigger IDs, which determine which survey is delivered from the HaTS
// backend.
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySettingsTriggerId{
        &kTrustSafetySentimentSurvey, "privacy-settings-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTrustedSurfaceTriggerId{
        &kTrustSafetySentimentSurvey, "trusted-surface-trigger-id", ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTransactionsTriggerId{
        &kTrustSafetySentimentSurvey, "transactions-trigger-id", ""};
// The time the user must remain on settings after interacting with a privacy
// setting to be considered.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyPrivacySettingsTime{&kTrustSafetySentimentSurvey,
                                                   "privacy-settings-time",
                                                   base::Seconds(20)};
// The time the user must have the Trusted Surface bubble open to be considered.
// Alternatively the user can interact with the bubble, in which case this time
// is irrelevant.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyTrustedSurfaceTime{
        &kTrustSafetySentimentSurvey, "trusted-surface-time", base::Seconds(5)};
// The time the user must remain on settings after visiting the password
// manager page.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyTransactionsPasswordManagerTime{
        &kTrustSafetySentimentSurvey, "transactions-password-manager-time",
        base::Seconds(20)};

#endif

// TrustSafetySentimentSurveyV2
#if !BUILDFLAG(IS_ANDROID)
// Enables the second version of the sentiment survey for users of Trust &
// Safety features, using HaTS.
BASE_FEATURE(kTrustSafetySentimentSurveyV2, base::FEATURE_ENABLED_BY_DEFAULT);
// The minimum and maximum time after a user has interacted with a Trust and
// Safety feature that they are eligible to be surveyed.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MinTimeToPrompt{
        &kTrustSafetySentimentSurveyV2, "min-time-to-prompt", base::Minutes(2)};
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MaxTimeToPrompt{&kTrustSafetySentimentSurveyV2,
                                                 "max-time-to-prompt",
                                                 base::Minutes(60)};
// The maximum and minimum range for the random number of NTPs that the user
// must at least visit after interacting with a Trust and Safety feature to be
// eligible for a survey.
const base::FeatureParam<int> kTrustSafetySentimentSurveyV2NtpVisitsMinRange{
    &kTrustSafetySentimentSurveyV2, "ntp-visits-min-range", 2};
const base::FeatureParam<int> kTrustSafetySentimentSurveyV2NtpVisitsMaxRange{
    &kTrustSafetySentimentSurveyV2, "ntp-visits-max-range", 4};
// The minimum time that has to pass in the current session before a user can be
// eligible to be considered for the baseline control group.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MinSessionTime{
        &kTrustSafetySentimentSurveyV2, "min-session-time", base::Seconds(30)};
// The feature area probabilities for each feature area considered as part of
// the Trust & Safety sentiment survey.
// TODO(crbug.com/40245476): Calculate initial probabilities and remove 0.0
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2BrowsingDataProbability{
        &kTrustSafetySentimentSurveyV2, "browsing-data-probability", 0.006};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2ControlGroupProbability{
        &kTrustSafetySentimentSurveyV2, "control-group-probability", 0.000025};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2DownloadWarningUIProbability{
        &kTrustSafetySentimentSurveyV2, "download-warning-ui-probability",
        0.05213384};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PasswordCheckProbability{
        &kTrustSafetySentimentSurveyV2, "password-check-probability", 0.195};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PasswordProtectionUIProbability{
        &kTrustSafetySentimentSurveyV2, "password-protection-ui-probability",
        0.5};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyCheckProbability{
        &kTrustSafetySentimentSurveyV2, "safety-check-probability", 0.12121};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyHubNotificationProbability{
        &kTrustSafetySentimentSurveyV2, "safety-hub-notification-probability",
        0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyHubInteractionProbability{
        &kTrustSafetySentimentSurveyV2, "safety-hub-interaction-probability",
        0.0};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2TrustedSurfaceProbability{
        &kTrustSafetySentimentSurveyV2, "trusted-surface-probability",
        0.012685};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacyGuideProbability{
        &kTrustSafetySentimentSurveyV2, "privacy-guide-probability", 0.5};
const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialProbability{
        &kTrustSafetySentimentSurveyV2,
        "safe-browsing-interstitial-probability", 0.18932671};
// The HaTS trigger IDs, which determine which survey is delivered from the HaTS
// backend.
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2BrowsingDataTriggerId{
        &kTrustSafetySentimentSurveyV2, "browsing-data-trigger-id",
        "1iSgej9Tq0ugnJ3q1cK0QwXZ12oo"};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2ControlGroupTriggerId{
        &kTrustSafetySentimentSurveyV2, "control-group-trigger-id",
        "CXMbsBddw0ugnJ3q1cK0QJM1Hu8m"};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2DownloadWarningUITriggerId{
        &kTrustSafetySentimentSurveyV2, "download-warning-ui-trigger-id",
        "7SS4sg4oR0ugnJ3q1cK0TNvCvd8U"};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PasswordCheckTriggerId{
        &kTrustSafetySentimentSurveyV2, "password-check-trigger-id",
        "Xd54YDVNJ0ugnJ3q1cK0UYBRruNH"};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PasswordProtectionUITriggerId{
        &kTrustSafetySentimentSurveyV2, "password-protection-ui-trigger-id",
        "bQBRghu5w0ugnJ3q1cK0RrqdqVRP"};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyCheckTriggerId{
        &kTrustSafetySentimentSurveyV2, "safety-check-trigger-id",
        "YSDfPVMnX0ugnJ3q1cK0RxEhwkay"};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyHubInteractionTriggerId{
        &kTrustSafetySentimentSurveyV2, "safety-hub-interaction-trigger-id",
        ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyHubNotificationTriggerId{
        &kTrustSafetySentimentSurveyV2, "safety-hub-notification-trigger-id",
        ""};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2TrustedSurfaceTriggerId{
        &kTrustSafetySentimentSurveyV2, "trusted-surface-trigger-id",
        "CMniDmzgE0ugnJ3q1cK0U6PaEn1f"};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacyGuideTriggerId{
        &kTrustSafetySentimentSurveyV2, "privacy-guide-trigger-id",
        "tqR1rjeDu0ugnJ3q1cK0P9yJEq7Z"};
const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialTriggerId{
        &kTrustSafetySentimentSurveyV2, "safe-browsing-interstitial-trigger-id",
        "Z9pSWP53n0ugnJ3q1cK0Y6YkGRpU"};
// The time the user must have the Trusted Surface bubble open to be considered.
// Alternatively the user can interact with the bubble, in which case this time
// is irrelevant.
const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2TrustedSurfaceTime{
        &kTrustSafetySentimentSurveyV2, "trusted-surface-time",
        base::Seconds(5)};
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWebAppPeriodicPreinstallUpdate, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebAppMigratePreinstalledChat, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebAppInstallDialog, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kWebAppInstallDialogWinPin, base::FEATURE_DISABLED_BY_DEFAULT);

// When this feature is enabled, the web app sync code will process the
// `migrated_from_manifest_id` field in its sync data to possibly treat new
// applications that come in via sync as being the result of a migration.
// Unless `blink::features::kWebAppMigrationApi` is enabled on some other
// synced profile, no such data should exist in sync.
BASE_FEATURE(kWebAppHandleAppMigrationViaSync,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kWebAppUpgradeToDatabaseVersion6,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWebium, base::FEATURE_DISABLED_BY_DEFAULT);
// Enables logging InitialWebUI-related metrics. The metrics are not necessary
// comes from WebUI but can also come from the C++ version of them.
// Defaults to enabled to also collect metrics for the C++ group.
// See crbug.com/448794588.
BASE_FEATURE(kInitialWebUIMetrics, base::FEATURE_ENABLED_BY_DEFAULT);
// When enable, the reload button will be replaced with the a WebView, and
// chrome://webui-toolbar.top-chrome will be loaded as the content.
// crbug.com/444358999
BASE_FEATURE(kWebUIReloadButton, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kWebUIToolbarProcessOverheadExperiment,
             base::FEATURE_DISABLED_BY_DEFAULT);
// Switches location bar over to a WebUI implementation.
// See crbug.com/470042732
BASE_FEATURE(kWebUILocationBar, base::FEATURE_DISABLED_BY_DEFAULT);

// When this is enabled, all the checks for enabled individual WebUI toolbar
// controls in chrome/browser/ui/ui_features.h will return true.
BASE_FEATURE(kWebUIToolbar, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the WebUI toolbar WebContents opts out of frame eviction.
BASE_FEATURE(kWebUIToolbarFrameEvictionOptOut,
             base::FEATURE_DISABLED_BY_DEFAULT);

// The following feature params control the crash recovery behavior of the Web
// UI reload button. If the renderer crashes, we will try to recover it by
// reloading the contents until the number of crashes reaches
// `kWebUIReloadButtonMaxCrashRecoveryTimes`. If the maximum number of crash
// counts is reached, no recovery will be attempted until
// `WebUIReloadButtonCrashRecoverRetryInterval` later. The counter will reset if
// there is no crash within `WebUIReloadButtonCrashRecoverResetInterval`.
// Here is an example with the default settings.
// - 00:00 renderer crashes
// - 00:00 recovery is triggered
// - 00:05 renderer crashes
// - 00:05 recovery is triggered
// - 00:16 renderer crashes
// - 00:16 recovery is triggered, as it has been >= 10s and the counter is reset
// - 00:17 renderer crashes
// - 00:17 recovery is triggered
// - 00:18 renderer crashes
// - 01:18 recovery is triggered, it's not immediate because the it exceeds the
//         max recovery times, but it will retry after 1 minute
const base::FeatureParam<int> kWebUIReloadButtonMaxCrashRecoveryTimes{
    &kWebUIReloadButton, "WebUIReloadButtonMaxCrashRecoveryTimes", 3};
const base::FeatureParam<base::TimeDelta>
    kWebUIReloadButtonCrashRecoverResetInterval{
        &kWebUIReloadButton, "WebUIReloadButtonCrashRecoverResetInterval",
        base::Seconds(10)};
const base::FeatureParam<base::TimeDelta>
    kWebUIReloadButtonCrashRecoverRetryInterval{
        &kWebUIReloadButton, "WebUIReloadButtonCrashRecoverRetryInterval",
        base::Minutes(1)};

// When enabled, initial WebUI renderers that become unresponsive will be
// restarted without showing the hung renderer dialog.
// `WebUIReloadButtonRestartUnresponsiveRenderersTimeout` controls the timeout
// for unresponsive renderers.
// See crbug.com/475397687.
const base::FeatureParam<bool> kWebUIReloadButtonRestartUnresponsive{
    &kWebUIReloadButton, "WebUIReloadButtonRestartUnresponsive", false};
const base::FeatureParam<base::TimeDelta>
    kWebUIReloadButtonRestartUnresponsiveRenderersTimeout{
        &kWebUIReloadButton,
        "WebUIReloadButtonRestartUnresponsiveRenderersTimeout",
        base::Seconds(15)};

// When this is enabled, the `BrowserView` will not show until the reload button
// has finished loading.
const base::FeatureParam<bool> kWebUIReloadButtonDeferBrowserViewShow{
    &kWebUIReloadButton, "WebUIReloadButtonDeferBrowserViewShow", false};
// When this is enabled, the WebUI toolbar will be pre-warmed during browser
// initialization.
const base::FeatureParam<bool> kWebUIReloadButtonPrewarmWebUI{
    &kWebUIReloadButton, "WebUIReloadButtonPrewarmWebUI", false};
// When this is enabled, the pre-warmed WebUI will also navigate immediately. It
// only takes effect when `WebUIReloadButtonPrewarmWebUI` is enabled.
const base::FeatureParam<bool> kWebUIReloadButtonPrewarmWebUIPreNavigate{
    &kWebUIReloadButton, "WebUIReloadButtonPrewarmWebUIPreNavigate", false};
// When this is enabled, the WebUI toolbar will be pre-warmed at profile ready
// time instead of browser initialization time. It only takes effect when
// `WebUIReloadButtonPrewarmWebUI` is enabled.
const base::FeatureParam<bool> kWebUIReloadButtonProfilePrewarming{
    &kWebUIReloadButton, "WebUIReloadButtonProfilePrewarming", false};
// When this is enabled, the reload button will be marked as visible until its
// first non-empty paint.
const base::FeatureParam<bool> kWebUIReloadButtonKeepVisibleUntilPaint{
    &kWebUIReloadButton, "WebUIReloadButtonKeepVisibleUntilPaint", false};
// When enabled, bypasses creating URL loader throttles for WebUI resources
// during startup.
const base::FeatureParam<bool> kWebUIReloadButtonBypassLoaderThrottles{
    &kWebUIReloadButton, "WebUIReloadButtonBypassLoaderThrottles", false};
// When enabled, the split tabs button will be replaced with WebUI loaded from
// chrome://webui-toolbar.top-chrome.
// crbug.com/470039098
BASE_FEATURE(kWebUISplitTabsButton, base::FEATURE_DISABLED_BY_DEFAULT);
// When enabled, the home button will be replaced with WebUI loaded from
// chrome://webui-toolbar.top-chrome.
// crbug.com/470039765
BASE_FEATURE(kWebUIHomeButton, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the battery saver button will be replaced with WebUI loaded
// from chrome://webui-toolbar.top-chrome. crbug.com/503821930
BASE_FEATURE(kWebUIBatterySaverButton, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the performance intervention button will be replaced with WebUI
// loaded from chrome://webui-toolbar.top-chrome. crbug.com/503822129
BASE_FEATURE(kWebUIPerformanceInterventionButton,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the app menu button will be replaced with WebUI loaded from
// chrome://webui-toolbar.top-chrome.
BASE_FEATURE(kWebUIAppMenuButton, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the back/forward buttons will be replaced with WebUI loaded
// from chrome://webui-toolbar.top-chrome.
// crbug.com/470038385
BASE_FEATURE(kWebUIBackForwardButton, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the pinned toolbar actions will be replaced with WebUI loaded
// from chrome://webui-toolbar.top-chrome.
// crbug.com/474061420
BASE_FEATURE(kWebUIPinnedToolbarActions, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the extensions container will be replaced with WebUI loaded
// from chrome://webui-toolbar.top-chrome.
BASE_FEATURE(kWebUIExtensionsContainer, base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // !BUILDFLAG(IS_ANDROID)

// Enables the User-Agent override fix for SearchPrefetch. This will work only
// if enabled together with `kPreloadingRespectUserAgentOverride`.
BASE_FEATURE(kRespectUserAgentOverrideInSearchPrefetch,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Restricts the WebUI scripts able to use the generated code cache according to
// embedder-specified heuristics.
BASE_FEATURE(kRestrictedWebUICodeCache, base::FEATURE_DISABLED_BY_DEFAULT);

// Defines a comma-separated list of resource names able to use the generated
// code cache when RestrictedWebUICodeCache is enabled.
const base::FeatureParam<std::string> kRestrictedWebUICodeCacheResources{
    &kRestrictedWebUICodeCache, "RestrictedWebUICodeCacheResources", ""};




#if !BUILDFLAG(IS_ANDROID)
// A feature to enable smart restart metrics collection. The collected metrics
// will be used to make informed decisions about the future of the smart restart
// feature.
BASE_FEATURE(kSmartRestartMetrics, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSmartRestart, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kSmartRestartDelay{
    &kSmartRestart, "restart_delay", base::Minutes(1)};

BASE_FEATURE(kSmartRestartLockScreen, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kSmartRestartLockScreenTabThreshold{
    &kSmartRestartLockScreen, "lock_tab_threshold", -1};

const base::FeatureParam<int> kSmartRestartLockScreenDisruptionThreshold{
    &kSmartRestartLockScreen, "lock_disruption_threshold", 2};

const base::FeatureParam<base::TimeDelta> kSmartRestartLockScreenDelay{
    &kSmartRestartLockScreen, "lock_restart_delay", base::Minutes(5)};

const base::FeatureParam<double> kSmartRestartLockBypassBeforeUnloadThreshold{
    &kSmartRestartLockScreen, "lock_bypass_beforeunload_threshold", -1.0};


// A feature to record the difference in the number of tabs and windows between
// the last session and the current session on restart.
BASE_FEATURE(kRecordTabWindowDiffOnRestart, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
