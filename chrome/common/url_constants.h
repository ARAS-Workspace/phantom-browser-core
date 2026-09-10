// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.
// Except for WebUI UI/Host/SubPage constants. Those go in
// chrome/common/webui_url_constants.h.
//
// - The constants are divided into sections: Cross platform, platform-specific,
//   and feature-specific.
// - When adding platform/feature specific constants, if there already exists an
//   appropriate #if block, use that.
// - Keep the constants sorted by name within its section.

#ifndef CHROME_COMMON_URL_CONSTANTS_H_
#define CHROME_COMMON_URL_CONSTANTS_H_

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "net/net_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/chrome_url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace chrome {

// "Learn more" URL linked in the dialog to cast using a code.
inline constexpr char kAccessCodeCastLearnMoreURL[] =
    "https://support.google.com/chrome/a/?p=cast_to_class_teacher";

// "Chrome Settings" URL for the appearance page.
inline constexpr char kBrowserSettingsSearchEngineURL[] =
    "chrome://settings/search";

// The URL for the help center article to show when no Cast destination has been
// found.
inline constexpr char kCastNoDestinationFoundURL[] =
    "https://support.google.com/chromecast/?p=no_cast_destination";

#if BUILDFLAG(IS_CHROMEOS)
// The URL for the WebHID API help center article.
inline constexpr char kChooserHidOverviewUrl[] =
    "https://support.google.com/chrome?p=webhid";

// The URL for the WebUsb help center article.
inline constexpr char kChooserUsbOverviewURL[] =
    "https://support.google.com/chrome?p=webusb";
#endif  // BUILDFLAG(IS_CHROMEOS)

// General help links for Chrome, opened using various actions.
#if BUILDFLAG(IS_CHROMEOS)
inline constexpr char kChromeHelpViaKeyboardURL[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook?p=help&ctx=keyboard";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

inline constexpr char kChromeHelpViaMenuURL[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook?p=help&ctx=menu";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

inline constexpr char kChromeHelpViaWebUIURL[] =
    "https://support.google.com/chrome?p=help&ctx=settings";
inline constexpr char kChromeOsHelpViaWebUIURL[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook?p=help&ctx=settings";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_CHROMEOS)

// The chrome-native: scheme is used show pages rendered with platform specific
// widgets instead of using HTML.
inline constexpr char kChromeNativeScheme[] = "chrome-native";

// Host and URL for most visited iframes used on the Instant Extended NTP.
inline constexpr char kChromeSearchMostVisitedHost[] = "most-visited";
inline constexpr char kChromeSearchMostVisitedUrl[] =
    "chrome-search://most-visited/";

// URL for NTP custom background image selected from the user's machine and
// filename for the version of the file in the Profile directory
inline constexpr char kChromeUIUntrustedNewTabPageBackgroundUrl[] =
    "chrome-untrusted://new-tab-page/background.jpg";
inline constexpr char kChromeUIUntrustedNewTabPageBackgroundFilename[] =
    "background.jpg";

// Page under chrome-search.
inline constexpr char kChromeSearchRemoteNtpHost[] = "remote-ntp";

// The chrome-search: scheme is served by the same backend as chrome:.  However,
// only specific URLDataSources are enabled to serve requests via the
// chrome-search: scheme.  See |InstantIOContext::ShouldServiceRequest| and its
// callers for details.  Note that WebUIBindings should never be granted to
// chrome-search: pages.  chrome-search: pages are displayable but not readable
// by external search providers (that are rendered by Instant renderer
// processes), and neither displayable nor readable by normal (non-Instant) web
// pages.  To summarize, a non-Instant process, when trying to access
// 'chrome-search://something', will bump up against the following:
//
//  1. Renderer: The display-isolated check in WebKit will deny the request,
//  2. Browser: Assuming they got by #1, the scheme checks in
//     URLDataSource::ShouldServiceRequest will deny the request,
//  3. Browser: for specific sub-classes of URLDataSource, like ThemeSource
//     there are additional Instant-PID checks that make sure the request is
//     coming from a blessed Instant process, and deny the request.
inline constexpr char kChromeSearchScheme[] = "chrome-search";

// This is the base URL of content that can be embedded in chrome://new-tab-page
// using an <iframe>. The embedded untrusted content can make web requests and
// can include content that is from an external source.
inline constexpr char kChromeUIUntrustedNewTabPageUrl[] =
    "chrome-untrusted://new-tab-page/";

// URL of the Google Account.
inline constexpr char kGoogleAccountURL[] = "https://myaccount.google.com";

// URL of the Google Account chooser.
inline constexpr char kGoogleAccountChooserURL[] =
    "https://accounts.google.com/AccountChooser";

// URL of the Google Account page showing the known user devices.
inline constexpr char kGoogleAccountDeviceActivityURL[] =
    "https://myaccount.google.com/device-activity?utm_source=chrome";

// URL of the two factor authentication setup required intersitial.
inline constexpr char kGoogleTwoFactorIntersitialURL[] =
    "https://myaccount.google.com/interstitials/twosvrequired";

// URL of the Google Password Manager.
inline constexpr char kGooglePasswordManagerURL[] = "";

// The URL for the "Learn more" page for the usage/crash reporting option in the
// first run dialog.
inline constexpr char kLearnMoreReportingURL[] =
    "https://support.google.com/chrome?p=ui_usagestat";

// The URL for the Learn More page about policies and enterprise enrollment.
inline constexpr char16_t kManagedUiLearnMoreUrl[] =
#if BUILDFLAG(IS_CHROMEOS)
    u"https://support.google.com/chromebook?p=is_chrome_managed";
#else
    u"https://support.google.com/chrome?p=is_chrome_managed";
#endif

inline constexpr char16_t kMyActivityUrlInHistory[] =
    u"https://myactivity.google.com/myactivity/?utm_source=chrome_h";

// The URL for "Your Gemini Apps Activity" page.
inline constexpr char16_t kMyActivityGeminiAppsUrl[] = u"";

// "Chrome Settings" URL for Ad Topics page
inline constexpr char kPrivacySandboxAdTopicsURL[] =
    "chrome://settings/adPrivacy/interests";

// "Chrome Settings" URL for Managing Topics page
inline constexpr char kPrivacySandboxManageTopicsURL[] =
    "chrome://settings/adPrivacy/interests/manage";

#if BUILDFLAG(IS_ANDROID)
// "Learn more" URL for unsafe site warnings.
inline constexpr char kUnsafeSiteWarningHelpCenterURL[] =
    "https://support.google.com/chrome?p=cpn_safe_browsing_wv";
#endif  // BUILDFLAG(IS_ANDROID)

// "Learn more" URL for safety tip bubble.
inline constexpr char kSafetyTipHelpCenterURL[] =
    "https://support.google.com/chrome?p=safety_tip";

// The URL for the Learn More page about Sync and Google services.
inline constexpr char kSyncAndGoogleServicesLearnMoreURL[] =
    "https://support.google.com/chrome?p=syncgoogleservices";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kSyncAndGoogleServicesLearnMoreURL) ==
              ash::chrome_external_urls::kSyncAndGoogleServicesLearnMoreURL);
#endif

// The URL for the "Learn more" page on sync encryption.
inline constexpr char16_t kSyncEncryptionHelpURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    u"https://support.google.com/chromebook?p=settings_encryption";
#else
    u"https://support.google.com/chrome?p=settings_encryption";
#endif

#if BUILDFLAG(IS_CHROMEOS)
// The URL for the "Learn more" link when there is a sync error.
inline constexpr char kSyncErrorsHelpURL[] =
    "https://support.google.com/chrome?p=settings_sync_error";
#endif

// Legacy URL to the sync google dashboard.
inline constexpr char kLegacySyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kLegacySyncGoogleDashboardURL) ==
              ash::chrome_external_urls::kLegacySyncGoogleDashboardURL);
#endif

// New URL to the sync google dashboard.
inline constexpr char kNewSyncGoogleDashboardURL[] =
    "https://chrome.google.com/data";
#if BUILDFLAG(IS_CHROMEOS)
static_assert(std::string_view(kNewSyncGoogleDashboardURL) ==
              ash::chrome_external_urls::kNewSyncGoogleDashboardURL);
#endif

// The URL for the "Learn more" page for signing in to chrome with expanded
// section on "Sign in and turn on sync" in the Computer/Desktop tab.
inline constexpr char kSigninOnDesktopLearnMoreURL[] =
    "https://support.google.com/"
    "chrome?p=settings_sign_in#zippy=sign-in-turn-on-sync";

// The URL for the "Learn more" page for adding a new profile to Chrome.
inline constexpr char kAddNewProfileOnDesktopLearnMoreURL[] =
    "https://support.google.com/chrome/?p=add_profile";

// The URL for the "Learn more" page for Help me Write.
inline constexpr char kComposeLearnMorePageURL[] =
    "https://support.google.com/chrome?p=help_me_write";

// The URL for the Settings page to enable history search.
inline constexpr char16_t kHistorySearchSettingURL[] =
    u"chrome://settings/ai/historySearch";

// The URL for the "Learn more" page for Wallpaper Search.
inline constexpr char kWallpaperSearchLearnMorePageURL[] =
    "https://support.google.com/chrome?p=create_themes_with_ai";

// The URL for the "Learn more" link in the enterprise disclaimer for managed
// profile in the Signin Intercept bubble.
inline constexpr char kSigninInterceptManagedDisclaimerLearnMoreURL[] =
    "https://support.google.com/chrome/a/?p=profile_separation";

inline constexpr char kUpgradeHelpCenterBaseURL[] =
    "https://support.google.com/installer/?product="
    "{8A69D345-D564-463c-AFF1-A69D9E530F96}&error=";

// The URL path to Google's Embedded Privacy Policy page.
inline constexpr char kPrivacyPolicyOnlineURLPath[] =
    "https://policies.google.com/privacy/embedded";

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// "Learn more" URL for the enhanced playback notification dialog.
inline constexpr char kEnhancedPlaybackNotificationLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS)
    "https://support.google.com/chromebook?p=enhanced_playback";
#else
    // Keep in sync with
    // chrome/browser/ui/android/strings/android_chrome_strings.grd
    "https://support.google.com/chrome?p=mobile_protected_content";
#endif
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Chrome OS default pre-defined custom handlers
inline constexpr char kChromeOSDefaultMailtoHandler[] =
    "https://mail.google.com/mail/?extsrc=mailto&amp;url=%s";
inline constexpr char kChromeOSDefaultWebcalHandler[] =
    "https://www.google.com/calendar/render?cid=%s";

// The URL for the "Account recovery" page.
inline constexpr char kAccountRecoveryURL[] =
    "https://accounts.google.com/signin/recovery";

// The URL for the Learn More page about enterprise enrolled devices.
inline constexpr char kLearnMoreEnterpriseURL[] =
    "https://support.google.com/chromebook?p=managed";

#endif  // BUILDFLAG(IS_CHROMEOS)

// Please do not append entries here. See the comments at the top of the file.

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
