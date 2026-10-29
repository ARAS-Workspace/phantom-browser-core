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

namespace chrome {

// "Chrome Settings" URL for the appearance page.
inline constexpr char kBrowserSettingsSearchEngineURL[] =
    "chrome://settings/search";

// The chrome-native: scheme is used show pages rendered with platform specific
// widgets instead of using HTML.
inline constexpr char kChromeNativeScheme[] = "chrome-native";

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

// The URL for the "Learn more" page for the usage/crash reporting option in the
// first run dialog.
inline constexpr char kLearnMoreReportingURL[] =
    "https://support.google.com/chrome?p=ui_usagestat";

// The URL for the Learn More page about policies and enterprise enrollment.
inline constexpr char16_t kManagedUiLearnMoreUrl[] =
    u"https://support.google.com/chrome?p=is_chrome_managed";

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

// "Learn more" URL for safety tip bubble.
inline constexpr char kSafetyTipHelpCenterURL[] =
    "https://support.google.com/chrome?p=safety_tip";

// The URL for the Learn More page about Sync and Google services.
inline constexpr char kSyncAndGoogleServicesLearnMoreURL[] =
    "https://support.google.com/chrome?p=syncgoogleservices";

// The URL for the "Learn more" page on sync encryption.
inline constexpr char16_t kSyncEncryptionHelpURL[] =
    u"https://support.google.com/chrome?p=settings_encryption";

// Legacy URL to the sync google dashboard.
inline constexpr char kLegacySyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync";

// New URL to the sync google dashboard.
inline constexpr char kNewSyncGoogleDashboardURL[] =
    "https://chrome.google.com/data";

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

// Please do not append entries here. See the comments at the top of the file.

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
