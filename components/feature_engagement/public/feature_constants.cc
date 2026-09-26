// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_constants.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace feature_engagement {

// Features used by the In-Product Help system.
BASE_FEATURE(kIPHDemoMode, "IPH_DemoMode", base::FEATURE_DISABLED_BY_DEFAULT);

// Features used by various clients to show their In-Product Help messages.
BASE_FEATURE(kIPHDummyFeature, "IPH_Dummy", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOnDeviceStorageEnabled() {
  return false;
}

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
BASE_FEATURE(kEsbDownloadRowPromoFeature,
             "EsbDownloadRowPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif
BASE_FEATURE(kIPHBatterySaverModeFeature,
             "IPH_BatterySaverMode",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHCompanionSidePanelFeature,
             "IPH_CompanionSidePanel",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHCompanionSidePanelRegionSearchFeature,
             "IPH_CompanionSidePanelRegionSearch",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHComposeMSBBSettingsFeature,
             "IPH_ComposeMSBBSettingsFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHComposeNewBadgeFeature,
             "IPH_ComposeNewBadgeFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHDesktopSharedHighlightingFeature,
             "IPH_DesktopSharedHighlighting",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHDesktopCustomizeChromeExperimentFeature,
             "IPH_DesktopCustomizeChromeExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHDesktopCustomizeChromeAutoOpenFeature,
             "IPH_DesktopCustomizeChromeAutoOpen",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHDesktopRealboxContextualSearchFeature,
             "IPH_DesktopRealboxContextualSearchFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHDiscardRingFeature,
             "IPH_DiscardRing",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHDownloadEsbPromoFeature,
             "IPH_DownloadEsbPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHExplicitBrowserSigninPreferenceRememberedFeature,
             "IPH_ExplicitBrowserSigninPreferenceRemembered",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHHistorySearchFeature,
             "IPH_HistorySearch",
             base::FEATURE_ENABLED_BY_DEFAULT);
#if BUILDFLAG(ENABLE_EXTENSIONS)
BASE_FEATURE(kIPHExtensionsMenuFeature,
             "IPH_ExtensionsMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHExtensionsRequestAccessButtonFeature,
             "IPH_ExtensionsRequestAccessButton",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHExtensionsZeroStatePromoFeature,
             "IPH_ExtensionsZeroStatePromo",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<IPHExtensionsZeroStatePromoVariant>::Option
    kIPHExtensionsZeroStatePromoVariantOptions[] = {
        {IPHExtensionsZeroStatePromoVariant::kCustomActionIph,
         "custom-action-iph"},
        {IPHExtensionsZeroStatePromoVariant::kCustomUiChipIphV1,
         "custom-ui-chip-iph"},
        {IPHExtensionsZeroStatePromoVariant::kCustomUiChipIphV2,
         "custom-ui-chip-iph-v2"},
        {IPHExtensionsZeroStatePromoVariant::kCustomUiChipIphV3,
         "custom-ui-chip-iph-v3"},
        {IPHExtensionsZeroStatePromoVariant::kCustomUIPlainLinkIph,
         "custom-ui-plain-link-iph"}};
BASE_FEATURE_ENUM_PARAM(
    IPHExtensionsZeroStatePromoVariant,
    kIPHExtensionsZeroStatePromoVariantParam,
    &feature_engagement::kIPHExtensionsZeroStatePromoFeature,
    "x_iph-variant",
    IPHExtensionsZeroStatePromoVariant::kCustomUiChipIphV2,
    &kIPHExtensionsZeroStatePromoVariantOptions);
#endif
BASE_FEATURE(kIPHFocusHelpBubbleScreenReaderPromoFeature,
             "IPH_FocusHelpBubbleScreenReaderPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHGMCCastStartStopFeature,
             "IPH_GMCCastStartStop",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHGMCLocalMediaCastingFeature,
             "IPH_GMCLocalMediaCasting",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHMemorySaverModeFeature,
             "IPH_HighEfficiencyMode",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHMultistepFilterPromoFeature,
             "IPH_MultistepFilterPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHLensOverlayFeature,
             "IPH_LensOverlay",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kIPHLensOverlayUrlAllowFilters{
    &feature_engagement::kIPHLensOverlayFeature, "x_url_allow_filters", "[]"};
const base::FeatureParam<std::string> kIPHLensOverlayUrlBlockFilters{
    &feature_engagement::kIPHLensOverlayFeature, "x_url_block_filters", "[]"};
const base::FeatureParam<std::string> kIPHLensOverlayUrlPathMatchAllowPatterns{
    &feature_engagement::kIPHLensOverlayFeature,
    "x_url_path_match_allow_patterns", "[]"};
const base::FeatureParam<std::string>
    kIPHLensOverlayUrlForceAllowedUrlMatchPatterns{
        &feature_engagement::kIPHLensOverlayFeature,
        "x_url_forced_allowed_match_patterns", "[]"};
const base::FeatureParam<std::string> kIPHLensOverlayUrlPathMatchBlockPatterns{
    &feature_engagement::kIPHLensOverlayFeature,
    "x_url_path_match_block_patterns", "[]"};
const base::FeatureParam<base::TimeDelta> kIPHLensOverlayDelayTime{
    &feature_engagement::kIPHLensOverlayFeature, "x_wait_time",
    base::Seconds(7)};
BASE_FEATURE(kIPHLensOverlayTranslateButtonFeature,
             "IPH_LensOverlayTranslateButton",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHTabAudioMutingFeature,
             "IPH_TabAudioMuting",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPasswordsSavePrimingPromoFeature,
             "IPH_PasswordsSavePrimingPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPasswordsSaveRecoveryPromoFeature,
             "IPH_PasswordsSaveRecoveryPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPasswordsManagementBubbleAfterSaveFeature,
             "IPH_PasswordsManagementBubbleAfterSave",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPasswordsManagementBubbleDuringSigninFeature,
             "IPH_PasswordsManagementBubbleDuringSignin",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPasswordsWebAppProfileSwitchFeature,
             "IPH_PasswordsWebAppProfileSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPasswordManagerShortcutFeature,
             "IPH_PasswordManagerShortcut",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPasswordSharingFeature,
             "IPH_PasswordSharingFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPdfInkSignaturesFeature,
             "IPH_PdfInkSignatures",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPdfSearchifyFeature,
             "IPH_PdfSearchifyFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPdfTextAnnotationsFeature,
             "IPH_PdfTextAnnotations",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPdfTranslateBubbleFeature,
             "IPH_PdfTranslateBubble",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPerformanceInterventionDialogFeature,
             "IPH_PerformanceInterventionDialogFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPowerBookmarksSidePanelFeature,
             "IPH_PowerBookmarksSidePanel",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPriceInsightsPageActionIconLabelFeature,
             "IPH_PriceInsightsPageActionIconLabelFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPriceTrackingEmailConsentFeature,
             "IPH_PriceTrackingEmailConsentFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPriceTrackingPageActionIconLabelFeature,
             "IPH_PriceTrackingPageActionIconLabelFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHReadingListDiscoveryFeature,
             "IPH_ReadingListDiscovery",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHReadingListEntryPointFeature,
             "IPH_ReadingListEntryPoint",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHReadingListInSidePanelFeature,
             "IPH_ReadingListInSidePanel",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHReadingModeKeyboardShortcutFeature,
             "IPH_ReadingModeKeyboardShortcut",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHReadingModePageActionLabelFeature,
             "IPH_ReadingModePageActionLabel",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHReadingModePresentationModeFeature,
             "IPH_ReadingModePresentationMode",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHReadingModeSidePanelFeature,
             "IPH_ReadingModeSidePanel",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHShoppingCollectionFeature,
             "IPH_ShoppingCollectionFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSmartTabSharingDefaultOnFeature,
             "IPH_SmartTabSharingDefaultOnFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSmartTabSharingFeature,
             "IPH_SmartTabSharingFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSmartTabSharingTryItFeature,
             "IPH_SmartTabSharingTryItFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSideBySidePinnableFeature,
             "IPH_SideBySidePinnableFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSideBySideTabSwitchFeature,
             "IPH_SideBySideTabSwitchFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSplitViewHorizontalIndirectAccessFeature,
             "IPH_SplitViewHorizontalIndirectAccessFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHVerticalTabsExpandOnHoverFeature,
             "IPH_VerticalTabsExpandOnHoverFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHVerticalTabstripTutorialFeature,
             "IPH_VerticalTabstripTutorialFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSidePanelGenericPinnableFeature,
             "IPH_SidePanelGenericPinnableFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSidePanelLensOverlayPinnableFeature,
             "IPH_SidePanelLensOverlayPinnableFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSidePanelLensOverlayPinnableFollowupFeature,
             "IPH_SidePanelLensOverlayPinnableFollowupFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSidePanelContextualTasksPinnableFeature,
             "IPH_SidePanelContextualTasksPinnableFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSideSearchAutoTriggeringFeature,
             "IPH_SideSearchAutoTriggering",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHSideSearchPageActionLabelFeature,
             "IPH_SideSearchPageActionLabel",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPwaQuietNotificationFeature,
             "IPH_PwaQuietNotification",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHTabGroupsSaveV2IntroFeature,
             "IPH_TabGroupsSaveV2Intro",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHTabGroupsSaveV2CloseGroupFeature,
             "IPH_TabGroupsSaveV2CloseGroup",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHTabGroupsSharedTabChangedFeature,
             "IPH_TabGroupsSharedTabChanged",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHTabGroupsSharedTabFeedbackFeature,
             "IPH_TabGroupsSharedTabFeedback",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHTabSearchComboButtonFeature,
             "IPH_TabSearchComboButton",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHDesktopSnoozeFeature,
             "IPH_DesktopSnoozeFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHDesktopPwaInstallFeature,
             "IPH_DesktopPwaInstall",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHProfileSwitchFeature,
             "IPH_ProfileSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHWebUiHelpBubbleTestFeature,
             "IPH_WebUiHelpBubbleTest",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHPriceTrackingInSidePanelFeature,
             "IPH_PriceTrackingInSidePanel",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHBackNavigationMenuFeature,
             "IPH_BackNavigationMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHBookmarkBarSimplifiedFeature,
             "IPH_BookmarkBarSimplified",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
BASE_FEATURE(kIPHExtensionsPinnedByDefaultFeature,
             "IPH_ExtensionsPinnedByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kIPHAutofillAtMemoryFeature,
             "IPH_AutofillAtMemory",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillBnplAffirmOrZipSuggestionFeature,
             "IPH_AutofillBnplAffirmOrZipSuggestion",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillBnplAffirmZipOrKlarnaSuggestionFeature,
             "IPH_AutofillBnplAffirmZipOrKlarnaSuggestion",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillCardInfoRetrievalSuggestionFeature,
             "IPH_AutofillCardInfoRetrievalSuggestion",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillCreditCardBenefitFeature,
             "IPH_AutofillCreditCardBenefit",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillDisabledVirtualCardSuggestionFeature,
             "IPH_AutofillDisabledVirtualCardSuggestion",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillDownstreamCardAwarenessFeature,
             "IPH_AutofillDownstreamCardAwareness",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillExternalAccountProfileSuggestionFeature,
             "IPH_AutofillExternalAccountProfileSuggestion",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillHomeWorkProfileSuggestionFeature,
             "IPH_AutofillHomeWorkProfileSuggestion",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillAccountNameEmailSuggestionFeature,
             "IPH_AutofillAccountNameEmailSuggestion",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillAiOptInFeature,
             "IPH_AutofillAiOptIn",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillAiValuablesFeature,
             "IPH_AutofillAiValuables",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillVirtualCardCVCSuggestionFeature,
             "IPH_AutofillVirtualCardCVCSuggestion",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillVirtualCardSuggestionFeature,
             "IPH_AutofillVirtualCardSuggestion",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillEnableLoyaltyCardsFeature,
             "IPH_AutofillEnableLoyaltyCards",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHAutofillOmniboxPaymentChipFeature,
             "IPH_AutofillOmniboxPaymentChip",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// This can be enabled by default, as the DesktopPWAsLinkCapturing
// flag is needed for the IPH linked to this feature to work, and
// use-cases to show the IPH are guarded by that flag.
BASE_FEATURE(kIPHDesktopPWAsLinkCapturingLaunch,
             "IPH_DesktopPWAsLinkCapturingLaunch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This can be enabled by default, as the DesktopPWAsLinkCapturing
// flag is needed for the IPH linked to this feature to work, and
// use-cases to show the IPH are guarded by that flag.
BASE_FEATURE(kIPHDesktopPWAsLinkCapturingLaunchAppInTab,
             "IPH_DesktopPWAsLinkCapturingLaunchAppInTab",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kIPHSignInBenefitsFeature,
             "IPH_SignInBenefits",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIPHSignInBenefitsNewSigninFeature,
             "IPH_SignInBenefitsNewSignin",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIPHSupervisedUserProfileSigninFeature,
             "IPH_SupervisedUserProfileSignin",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

BASE_FEATURE(kIPHiOSPasswordPromoDesktopFeature,
             "IPH_iOSPasswordPromoDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHiOSAddressPromoDesktopFeature,
             "IPH_iOSAddressPromoDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHiOSPaymentPromoDesktopFeature,
             "IPH_iOSPaymentPromoDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHiOSLensPromoDesktopFeature,
             "IPH_iOSLensPromoDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHiOSEnhancedBrowsingDesktopFeature,
             "IPH_iOSEnhancedBrowsingDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHiOSTabGroupsDesktopFeature,
             "IPH_iOSTabGroupsDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHiOSPriceTrackingDesktopFeature,
             "IPH_iOSPriceTrackingDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIPHResumptionRailFeature,
             "IPH_ResumptionRail",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace feature_engagement
