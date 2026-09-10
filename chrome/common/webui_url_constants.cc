// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"

#include <array>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"

namespace chrome {

// Note: Add hosts to `ChromeURLHosts()` at the bottom of this file to be listed
// by chrome://chrome-urls (about:about) and the built-in AutocompleteProvider.

// Add hosts here to be suggested by BuiltinProvider.
base::span<const base::cstring_view> ChromeURLHosts() {
  static constexpr auto kChromeURLHosts = std::to_array<base::cstring_view>({
      kChromeUIAboutHost,
      kChromeUIAccessibilityHost,
      kChromeUIActorInternalsHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIAppServiceInternalsHost,
#endif
      kChromeUIChromeFindsInternalsHost,
      kChromeUIChromeURLsHost,
      kChromeUIComponentsHost,
      kChromeUIConnectorsInternalsHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIContextualCueingInternalsHost,
#endif
      kChromeUICrashesHost,
      kChromeUICreditsHost,
      kChromeUICrossDeviceSigninQrBubbleHost,
      kChromeUIDownloadInternalsHost,
      kChromeUIFamilyLinkUserInternalsHost,
      kChromeUIFlagsHost,
      kChromeUIHistoryHost,
      history_clusters_internals::kChromeUIHistoryClustersInternalsHost,
      kChromeUIInterstitialHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIIwaDevHost,
#endif
      kChromeUILocalStateHost,
      kChromeUIMediaEngagementHost,
      kChromeUIMetricsInternalsHost,
      kChromeUINetExportHost,
      kChromeUINetInternalsHost,
      kChromeUINewTabHost,
      kChromeUIOmniboxHost,
      kChromeUIOmniboxAimEligibilityPage,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIOnDeviceInternalsHost,
#endif
      optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost,
      kChromeUIPolicyHost,
      kChromeUIPredictorsHost,
      kChromeUIPrefsInternalsHost,
      kChromeUIProfileInternalsHost,
      content::kChromeUIQuotaInternalsHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIWebUIToolbarHost,
#endif
      kChromeUISignInInternalsHost,
      kChromeUISiteEngagementHost,
      kChromeUISkillsHost,
      kChromeUISubresourceFilterInternalsHost,
      kChromeUINTPTilesInternalsHost,
      safe_browsing::kChromeUISafeBrowsingHost,
      kChromeUISyncInternalsHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUITabSearchHost,
      kChromeUITabsFromOtherDevicesSidePanelHost,
      kChromeUITermsHost,
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#endif
      kChromeUIUserActionsHost,
      kChromeUIVersionHost,
#if !BUILDFLAG(IS_ANDROID)
#endif
      content::kChromeUIBlobInternalsHost,
      content::kChromeUIDinoHost,
      content::kChromeUIGpuHost,
      content::kChromeUIHistogramHost,
      content::kChromeUIIndexedDBInternalsHost,
      content::kChromeUIMediaInternalsHost,
      content::kChromeUINetworkErrorsListingHost,
      content::kChromeUIProcessInternalsHost,
      content::kChromeUIServiceWorkerInternalsHost,
#if !BUILDFLAG(IS_ANDROID)
      content::kChromeUITracingHost,
#endif
      content::kChromeUIUkmHost,
      content::kChromeUIWebRTCInternalsHost,
#if BUILDFLAG(ENABLE_VR)
      content::kChromeUIWebXrInternalsHost,
#endif
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIAppLauncherPageHost,
      kChromeUIBookmarksHost,
      kChromeUIDownloadsHost,
      kChromeUIHelpHost,
      kChromeUIInspectHost,
      kChromeUINewTabPageHost,
      kChromeUINewTabPageThirdPartyHost,
#if BUILDFLAG(IS_ANDROID)
      kChromeUINotificationsInternalsHost,
#endif
      kChromeUISettingsHost,
      kChromeUISystemInfoHost,
      kChromeUIWhatsNewHost,
#endif
#if BUILDFLAG(IS_ANDROID)
      kChromeUISnippetsInternalsHost,
      kChromeUIWebApksHost,
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_DESKTOP_ANDROID)
      kChromeUIDiscardsHost,
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#endif
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
      kChromeUILinuxProxyConfigHost,
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
      kChromeUISandboxHost,
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      kChromeUIExtensionsHost,
      kChromeUIExtensionsInternalsHost,
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
      kChromeUIPrintHost,
#endif
      kChromeUIWebRtcLogsHost,
#if !BUILDFLAG(IS_ANDROID)
      kChromeUIOrganizerPanelHost,
      kChromeUIWebuiBrowserHost,
#endif  // !BUILDFLAG(IS_ANDROID)
  });

  return base::span(kChromeURLHosts);
}

base::span<const base::cstring_view> ChromeDebugURLs() {
  // TODO(crbug.com/40253037): make this list comprehensive
  static constexpr auto kChromeDebugURLs = std::to_array<base::cstring_view>(
      {blink::kChromeUIBadCastCrashURL,
       blink::kChromeUIBrowserCrashURL,
       blink::kChromeUIBrowserDcheckURL,
       blink::kChromeUIBrowserUIHang,
       blink::kChromeUIBrowserHeapMemberDerefAfterFreeURL,
       blink::kChromeUIBrowserHeapOverflowURL,
       blink::kChromeUIBrowserHeapUaFURL,
       blink::kChromeUIBrowserHeapUnderflowURL,
       blink::kChromeUIGpuHeapMemberDerefAfterFreeURL,
       blink::kChromeUIGpuHeapOverflowURL,
       blink::kChromeUIGpuHeapUaFURL,
       blink::kChromeUIGpuHeapUnderflowURL,
       blink::kChromeUIRendererHeapMemberDerefAfterFreeURL,
       blink::kChromeUIRendererHeapOverflowURL,
       blink::kChromeUIRendererHeapUaFURL,
       blink::kChromeUIRendererHeapUnderflowURL,
       blink::kChromeUICrashURL,
       blink::kChromeUICrashRustURL,
#if defined(ADDRESS_SANITIZER)
       blink::kChromeUICrashRustOverflowURL,
#endif
       blink::kChromeUIDumpURL,
       blink::kChromeUIKillURL,
       blink::kChromeUIHangURL,
       blink::kChromeUIShorthangURL,
       blink::kChromeUIGpuCleanURL,
       blink::kChromeUIGpuCrashURL,
       blink::kChromeUIGpuHangURL,
       blink::kChromeUIMemoryExhaustURL,
       blink::kChromeUIMemoryPressureCriticalURL,
       blink::kChromeUIMemoryPressureModerateURL,
#if BUILDFLAG(IS_ANDROID)
       blink::kChromeUIGpuJavaCrashURL,
       kChromeUIJavaCrashURL,
#else
       kChromeUIWebUIJsErrorURL,
#endif  // BUILDFLAG(IS_ANDROID)
       kChromeUIQuitURL,
       kChromeUIRestartURL});

  return base::span(kChromeDebugURLs);
}

const GURL& ChromeUINewTabPageURLAsGURL() {
  static base::NoDestructor<GURL> instance(kChromeUINewTabPageURL);
  return *instance;
}

const GURL& ChromeUINewTabURLAsGURL() {
  static base::NoDestructor<GURL> instance(kChromeUINewTabURL);
  return *instance;
}

}  // namespace chrome
