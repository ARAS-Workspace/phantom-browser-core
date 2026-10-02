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
      kChromeUIAppServiceInternalsHost,
      kChromeUIChromeFindsInternalsHost,
      kChromeUIChromeURLsHost,
      kChromeUIComponentsHost,
      kChromeUICrashesHost,
      kChromeUICreditsHost,
      kChromeUICrossDeviceSigninQrBubbleHost,
      kChromeUIDownloadInternalsHost,
      kChromeUIFamilyLinkUserInternalsHost,
      kChromeUIFlagsHost,
      kChromeUIHistoryHost,
      history_clusters_internals::kChromeUIHistoryClustersInternalsHost,
      kChromeUIInterstitialHost,
      kChromeUIIwaDevHost,
      kChromeUILocalStateHost,
      kChromeUIMediaEngagementHost,
      kChromeUIMetricsInternalsHost,
      kChromeUINetExportHost,
      kChromeUINetInternalsHost,
      kChromeUINewTabHost,
      kChromeUIOmniboxHost,
      kChromeUIOmniboxAimEligibilityPage,
      kChromeUIOnDeviceInternalsHost,
      kChromeUIPolicyHost,
      kChromeUIPredictorsHost,
      kChromeUIPrefsInternalsHost,
      kChromeUIProfileInternalsHost,
      content::kChromeUIQuotaInternalsHost,
      kChromeUIWebUIToolbarHost,
      kChromeUISignInInternalsHost,
      kChromeUISiteEngagementHost,
      kChromeUISkillsHost,
      kChromeUISubresourceFilterInternalsHost,
      kChromeUINTPTilesInternalsHost,
      safe_browsing::kChromeUISafeBrowsingHost,
      kChromeUISyncInternalsHost,
      kChromeUITabSearchHost,
      kChromeUITabsFromOtherDevicesSidePanelHost,
      kChromeUITermsHost,
      kChromeUIUserActionsHost,
      kChromeUIVersionHost,
      content::kChromeUIBlobInternalsHost,
      content::kChromeUIDinoHost,
      content::kChromeUIGpuHost,
      content::kChromeUIHistogramHost,
      content::kChromeUIIndexedDBInternalsHost,
      content::kChromeUIMediaInternalsHost,
      content::kChromeUINetworkErrorsListingHost,
      content::kChromeUIProcessInternalsHost,
      content::kChromeUIServiceWorkerInternalsHost,
      content::kChromeUITracingHost,
      content::kChromeUIUkmHost,
      content::kChromeUIWebRTCInternalsHost,
#if BUILDFLAG(ENABLE_VR)
      content::kChromeUIWebXrInternalsHost,
#endif
      kChromeUIAppLauncherPageHost,
      kChromeUIBookmarksHost,
      kChromeUIDownloadsHost,
      kChromeUIHelpHost,
      kChromeUIInspectHost,
      kChromeUINewTabPageHost,
      kChromeUINewTabPageThirdPartyHost,
      kChromeUISettingsHost,
      kChromeUISystemInfoHost,
      kChromeUIWhatsNewHost,
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      kChromeUIDiscardsHost,
#endif
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
      kChromeUILinuxProxyConfigHost,
#endif
#if BUILDFLAG(IS_LINUX)
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
      kChromeUIOrganizerPanelHost,
      kChromeUIWebuiBrowserHost,
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
       kChromeUIWebUIJsErrorURL,
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
