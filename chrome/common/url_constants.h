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

// The URL for "Your Gemini Apps Activity" page.
inline constexpr char16_t kMyActivityGeminiAppsUrl[] = u"";

// "Learn more" URL for safety tip bubble.
inline constexpr char kSafetyTipHelpCenterURL[] =
    "https://support.google.com/chrome?p=safety_tip";

// The URL for the "Learn more" page on sync encryption.
inline constexpr char16_t kSyncEncryptionHelpURL[] =
    u"https://support.google.com/chrome?p=settings_encryption";

// Legacy URL to the sync google dashboard.
inline constexpr char kLegacySyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync";

// New URL to the sync google dashboard.
inline constexpr char kNewSyncGoogleDashboardURL[] =
    "https://chrome.google.com/data";

// The URL for the "Learn more" link in the enterprise disclaimer for managed
// profile in the Signin Intercept bubble.
inline constexpr char kSigninInterceptManagedDisclaimerLearnMoreURL[] =
    "https://support.google.com/chrome/a/?p=profile_separation";

// Please do not append entries here. See the comments at the top of the file.

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
