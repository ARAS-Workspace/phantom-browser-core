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

BASE_FEATURE(kCaptureHandleForStandalonePwasAndIwas,
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Whether to allow installed-by-default web apps to be installed or not.
BASE_FEATURE(kPreinstalledWebAppInstallation,
             "DefaultWebAppInstallation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to force migrate preinstalled web apps whenever the old Chrome app
// they're replacing is detected, even if the web app is already installed.
// Used by unit tests.
BASE_FEATURE(kPreinstalledWebAppAlwaysMigrateForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables the `google-chrome://` URI scheme.
BASE_FEATURE(kGoogleChromeScheme, base::FEATURE_ENABLED_BY_DEFAULT);

// Force Privacy Guide to be available even if it would be unavailable
// otherwise. This is meant for development and test purposes only.
BASE_FEATURE(kPrivacyGuideForceAvailable, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables HTTPS-First Mode in a balanced configuration that doesn't warn on
// HTTP when HTTPS can't be reasonably expected.
BASE_FEATURE(kHttpsFirstBalancedMode, base::FEATURE_ENABLED_BY_DEFAULT);

// Automatically enables HTTPS-First Mode in a balanced configuration when
// possible.
BASE_FEATURE(kHttpsFirstBalancedModeAutoEnable,
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables Isolated Web App Developer Mode, which allows developers to
// install untrusted Isolated Web Apps.
BASE_FEATURE(kIsolatedWebAppDevMode, base::FEATURE_DISABLED_BY_DEFAULT);

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

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
BASE_FEATURE(kOnConnectNative, base::FEATURE_DISABLED_BY_DEFAULT);
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
BASE_FEATURE(kSitePerProcess, base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kProcessPerSiteForDSE, base::FEATURE_ENABLED_BY_DEFAULT);

// Consider the default search engine (DSE) warmup page as a search results page
// (SRP), for the purpose of applying the "process per site for DSE SRP" policy
// (`kProcessPerSiteForDSE`).
BASE_FEATURE(kConsiderDSEWarmUpPageAsSRP, base::FEATURE_ENABLED_BY_DEFAULT);

// Disable downloads of unsafe file types over insecure transports if initiated
// from a secure page. As of M89, mixed downloads are blocked on all platforms.
BASE_FEATURE(kTreatUnsafeDownloadsAsActive, base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kWebAppUpgradeToDatabaseVersion6,
             base::FEATURE_ENABLED_BY_DEFAULT);

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

}  // namespace features
