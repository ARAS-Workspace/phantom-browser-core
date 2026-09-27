// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "extensions/buildflags/buildflags.h"

namespace feature_engagement {

// Returns true if adding on-device storage is enabled.
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
bool IsOnDeviceStorageEnabled();

#define FEATURE_CONSTANTS_DECLARE_FEATURE(feature_name)  \
  COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS) \
  BASE_DECLARE_FEATURE(feature_name)

// A feature for enabling a demonstration mode for In-Product Help (IPH).
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDemoMode);

// A feature to ensure all arrays can contain at least one feature.
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDummyFeature);

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
FEATURE_CONSTANTS_DECLARE_FEATURE(kEsbDownloadRowPromoFeature);
#endif
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHBatterySaverModeFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHCompanionSidePanelFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHCompanionSidePanelRegionSearchFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHComposeMSBBSettingsFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHComposeNewBadgeFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDesktopSharedHighlightingFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDesktopCustomizeChromeExperimentFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDesktopCustomizeChromeAutoOpenFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDesktopRealboxContextualSearchFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDiscardRingFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDownloadEsbPromoFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(
    kIPHExplicitBrowserSigninPreferenceRememberedFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHHistorySearchFeature);
#if BUILDFLAG(ENABLE_EXTENSIONS)
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHExtensionsMenuFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHExtensionsRequestAccessButtonFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHExtensionsZeroStatePromoFeature);
// The variant of In-Product-Help (IPH) shown to users with zero extensions
// installed.
enum IPHExtensionsZeroStatePromoVariant {
  // A custom action IPH. Triggering the action opens a new tab to the Chrome
  // Web Store home page.
  kCustomActionIph,
  // A custom UI IPH, presenting the user with different collections of
  // extension collections in cr-chip buttons.
  kCustomUiChipIphV1,
  // Same as above, but with a slightly different color scheme to highlight
  // the chips button, and a different selection of links.
  kCustomUiChipIphV2,
  // Same as V2, but with a slightly different selection of links and orders.
  kCustomUiChipIphV3,
  // A custom UI IPH, presenting the user with different collections of
  // extension collections in plain text links.
  kCustomUIPlainLinkIph,
};
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
BASE_DECLARE_FEATURE_PARAM(IPHExtensionsZeroStatePromoVariant,
                           kIPHExtensionsZeroStatePromoVariantParam);
#endif
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHFocusHelpBubbleScreenReaderPromoFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHGMCCastStartStopFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHGMCLocalMediaCastingFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHMemorySaverModeFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHMultistepFilterPromoFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHLensOverlayFeature);
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
extern const base::FeatureParam<std::string> kIPHLensOverlayUrlAllowFilters;
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
extern const base::FeatureParam<std::string> kIPHLensOverlayUrlBlockFilters;
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
extern const base::FeatureParam<std::string>
    kIPHLensOverlayUrlPathMatchAllowPatterns;
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
extern const base::FeatureParam<std::string>
    kIPHLensOverlayUrlForceAllowedUrlMatchPatterns;
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
extern const base::FeatureParam<std::string>
    kIPHLensOverlayUrlPathMatchBlockPatterns;
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
extern const base::FeatureParam<base::TimeDelta> kIPHLensOverlayDelayTime;
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHLensOverlayTranslateButtonFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHTabAudioMutingFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPasswordsSavePrimingPromoFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPasswordsSaveRecoveryPromoFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(
    kIPHPasswordsManagementBubbleAfterSaveFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(
    kIPHPasswordsManagementBubbleDuringSigninFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPasswordsWebAppProfileSwitchFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPasswordManagerShortcutFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPasswordSharingFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPdfInkSignaturesFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPdfSearchifyFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPdfTextAnnotationsFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPdfTranslateBubbleFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPerformanceInterventionDialogFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPowerBookmarksSidePanelFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPriceInsightsPageActionIconLabelFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPriceTrackingEmailConsentFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPriceTrackingPageActionIconLabelFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHReadingListDiscoveryFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHReadingListEntryPointFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHReadingListInSidePanelFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHReadingModeKeyboardShortcutFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHReadingModePageActionLabelFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHReadingModePresentationModeFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHReadingModeSidePanelFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHShoppingCollectionFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSmartTabSharingDefaultOnFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSmartTabSharingFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSmartTabSharingTryItFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSideBySidePinnableFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSideBySideTabSwitchFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSplitViewHorizontalIndirectAccessFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHVerticalTabsExpandOnHoverFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHVerticalTabstripTutorialFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSidePanelGenericPinnableFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSidePanelLensOverlayPinnableFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(
    kIPHSidePanelLensOverlayPinnableFollowupFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSidePanelContextualTasksPinnableFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSideSearchAutoTriggeringFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSideSearchPageActionLabelFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPwaQuietNotificationFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHTabGroupsSaveV2IntroFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHTabGroupsSaveV2CloseGroupFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHTabGroupsSharedTabChangedFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHTabGroupsSharedTabFeedbackFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHTabSearchComboButtonFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDesktopSnoozeFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDesktopPwaInstallFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHProfileSwitchFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHWebUiHelpBubbleTestFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHPriceTrackingInSidePanelFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHBackNavigationMenuFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHBookmarkBarSimplifiedFeature);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHExtensionsPinnedByDefaultFeature);
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHAutofillAtMemoryFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHAutofillBnplAffirmOrZipSuggestionFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(
    kIPHAutofillBnplAffirmZipOrKlarnaSuggestionFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(
    kIPHAutofillCardInfoRetrievalSuggestionFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHAutofillCreditCardBenefitFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(
    kIPHAutofillDisabledVirtualCardSuggestionFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(
    kIPHAutofillExternalAccountProfileSuggestionFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHAutofillHomeWorkProfileSuggestionFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(
    kIPHAutofillAccountNameEmailSuggestionFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHAutofillAiOptInFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHAutofillAiValuablesFeature);
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHAutofillVirtualCardCVCSuggestionFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHAutofillVirtualCardSuggestionFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHAutofillEnableLoyaltyCardsFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHAutofillDownstreamCardAwarenessFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHAutofillOmniboxPaymentChipFeature);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDesktopPWAsLinkCapturingLaunch);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHDesktopPWAsLinkCapturingLaunchAppInTab);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSignInBenefitsFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSignInBenefitsNewSigninFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHSupervisedUserProfileSigninFeature);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHiOSPasswordPromoDesktopFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHiOSAddressPromoDesktopFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHiOSPaymentPromoDesktopFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHiOSLensPromoDesktopFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHiOSEnhancedBrowsingDesktopFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHiOSTabGroupsDesktopFeature);
FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHiOSPriceTrackingDesktopFeature);

FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHResumptionRailFeature);

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_
