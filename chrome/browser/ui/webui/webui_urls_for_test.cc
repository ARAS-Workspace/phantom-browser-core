// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_urls_for_test.h"

#include <string_view>

#include "base/containers/span.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

base::span<const std::string_view> GetChromeUrlsForTest() {
  // Using a hardcoded list because this is used to parameterize tests,
  // and test parameters cannot be generated at runtime from the
  // WebUIConfigMap. The WebUIUrlBrowserTest.UrlsInTestList test validates
  // that all URLs registered in the config map at runtime are added to
  // one of the lists below.
  static constexpr std::string_view kChromeUrls[] = {
#if defined(NDEBUG)
      // TODO(crbug.com/487113801): Investigate why tests are flaky on dbg bots.
      "chrome://accessibility",
#endif
      // TODO:(https://crbug.com/40265685): Flakily crashes on ChromeOS.
      "chrome://app-service-internals",

      "chrome://bookmarks",
      "chrome://bookmarks-side-panel.top-chrome",
      "chrome://certificate-manager",
      "chrome://chrome-finds-internals",
      "chrome://chrome-urls",
      "chrome://color-pipeline-internals",
      "chrome://comments-side-panel.top-chrome",
      "chrome://components",
      "chrome://connection-help",
      "chrome://connection-monitoring-detected",
      "chrome://connectors-internals",
      "chrome://content-settings",
      "chrome://crashes",
// TODO(crbug.com/40913109): Re-enable this test
#if !BUILDFLAG(IS_LINUX)
      "chrome://credits",
#endif
      "chrome://customize-chrome-side-panel.top-chrome",
      "chrome://data-sharing-internals",

      "chrome://default-browser-modal",

      "chrome://debug-webuis-disabled",
      "chrome://download-internals",
      "chrome://downloads",
      "chrome://extensions",
      "chrome://extensions-internals",
      "chrome://extensions-zero-state",
      "chrome://family-link-user-internals",
      "chrome://flags",
      "chrome://gpu",
      "chrome://histograms",
      "chrome://history",
      "chrome://history-clusters-internals",
      "chrome://history-clusters-side-panel.top-chrome",
      "chrome://history-side-panel.top-chrome",
      "chrome://indexeddb-internals",
      "chrome://infobar-internals",
      "chrome://inspect",
#if !BUILDFLAG(IS_ANDROID)
      "chrome://iwa-dev",
#endif
      "chrome://internals/session-service",
      "chrome://interstitials",
      "chrome://interstitials/ssl",
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
      "chrome://linux-proxy-config",
#endif
      "chrome://local-state",
      "chrome://media-engagement",
      "chrome://media-internals",
      "chrome://metrics-internals",
      "chrome://net-export",
      "chrome://net-internals",
      "chrome://network-errors",
      "chrome://new-tab-page",
      "chrome://newtab-footer",
      "chrome://new-tab-page-third-party",
      "chrome://newtab",
      "chrome://ntp-tiles-internals",
      "chrome://omnibox",
#if !BUILDFLAG(IS_ANDROID)
      "chrome://organizer-panel.top-chrome",
#endif
      "chrome://policy",
      "chrome://predictors",

  // TODO(crbug.com/511254271): Flaky on some Linux builders.
#if !BUILDFLAG(IS_LINUX)
      "chrome://prefs-internals",
#endif

      "chrome://process-internals",
      "chrome://profile-internals",
      "chrome://quota-internals",
      "chrome://read-later.top-chrome",
      "chrome://regional-capabilities-internals",
      "chrome://reset-password",
      "chrome://safe-browsing",
      "chrome://saved-tab-groups-unsupported",
      "chrome://search-engine-choice",
      "chrome://serviceworker-internals",
      "chrome://segmentation-internals",
      "chrome://settings",
      "chrome://shopping-insights-side-panel.top-chrome",
      "chrome://signin-internals",
      "chrome://site-engagement",
      "chrome://subresource-filter-internals",
      "chrome://support-tool",
      "chrome://sync-internals",
      "chrome://system",
      "chrome://tab-search.top-chrome",
      "chrome://tab-strip-internals",
      "chrome://tabs-from-other-devices.top-chrome",
      "chrome://terms",
      "chrome://traces",
      "chrome://traces-internals",
      "chrome://tracing",
      "chrome://ukm",
      "chrome://user-actions",
      "chrome://user-education-internals",
      "chrome://version",
      "chrome://webrtc-internals",
      "chrome://webrtc-logs",
      "chrome://webui-gallery",

#if BUILDFLAG(ENABLE_VR)
      "chrome://webxr-internals",
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      "chrome://whats-new",
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      "chrome://cast-feedback",
#endif

#if BUILDFLAG(IS_ANDROID)
      "chrome://explore-sites-internals",
      "chrome://internals/notifications",
      "chrome://internals/query-tiles",
      "chrome://snippets-internals",
      "chrome://webapks",
#endif

      "chrome://browser-switch",
      "chrome://browser-switch/internals",
      "chrome://profile-picker",
      "chrome://intro",
      "chrome://profile-customization/?debug",
      "chrome://signin-email-confirmation",
#if !BUILDFLAG(IS_MAC)
      "chrome://sandbox",
#endif  // !BUILDFLAG(IS_MAC)
#if !BUILDFLAG(IS_MAC)
      // TODO(crbug.com/40772380): this test is flaky on mac.
      "chrome://bluetooth-internals",
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      "chrome://signin-dice-web-intercept.top-chrome/?debug",
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      "chrome://signout-confirmation",
#endif
      "chrome://webuijserror",
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
      "chrome://print",
#endif
  };
  return kChromeUrls;
}

base::span<const std::string_view> GetUntestedChromeUrlsForTest() {
  // Don't add new URLs here unless there's a strong reason for them to be
  // exempted from these basic checks.
  static constexpr std::string_view kChromeUntestedUrls[] = {
      "chrome-untrusted://data-sharing",
      "chrome-untrusted://ntp-microsoft-auth",
      "chrome-untrusted://print",
      "chrome://access-code-cast",
#if !defined(NDEBUG)
      // TODO(crbug.com/487113801): Investigate why tests are flaky on dbg bots.
      "chrome://accessibility",
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      "chrome://cross-device-signin-qr-bubble",
#endif
      "chrome://constrained-test",
#if BUILDFLAG(IS_LINUX)
      // TODO(crbug.com/40913109): Re-enable this test
      "chrome://credits",
#endif
      // TODO(crbug.com/40710256): Test failure due to excessive output.
      "chrome://discards",
      "chrome://history-sync-optin",
      // Not a valid URL; only internals/session-service is valid.
      "chrome://internals",
      // Note: Disabled because a DCHECK fires when directly visiting the URL.
      "chrome://managed-user-profile-notice",
      // TODO(crbug.com/40185163): DCHECK failure
      "chrome://memory-internals",
      "chrome://omnibox-everywhere.top-chrome",
      "chrome://omnibox-popup.top-chrome",
      "chrome://profile-customization",
      "chrome://signin-dice-web-intercept.top-chrome",
      "chrome://signin-error",
      // TODO(crbug.com/40137561): Navigating to chrome://sync-confirmation and
      // quickly navigating away cause DCHECK failure.
      "chrome://sync-confirmation",
      "chrome://tab-group-home",
      "chrome://view-cert",
      "chrome://watermark",
      "chrome://webui-browser",
      "chrome://webui-toolbar.top-chrome",
  };
  return kChromeUntestedUrls;
}
