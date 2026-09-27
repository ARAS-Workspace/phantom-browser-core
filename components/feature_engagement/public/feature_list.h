// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_

#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/webui/flags/feature_entry.h"
#include "extensions/buildflags/buildflags.h"

namespace feature_engagement {
using FeatureVector = std::vector<const base::Feature*>;

// The param name for the FeatureVariation configuration, which is used by
// chrome://flags to set the variable name for the selected feature. The Tracker
// backend will then read this to figure out which feature (if any) was selected
// by the end user.
inline constexpr char kIPHDemoModeFeatureChoiceParam[] = "chosen_feature";

// Defines a const flags_ui::FeatureEntry::FeatureParam for the given
// base::Feature. The constant name will be on the form
// kFooFeature --> kFooFeatureVariation. The |feature_name| argument must
// match the base::Feature::name member of the |base_feature|.
// This is intended to be used with VARIATION_ENTRY below to be able to insert
// it into an array of flags_ui::FeatureEntry::FeatureVariation.
#define DEFINE_VARIATION_PARAM(base_feature, feature_name)     \
  using validate_##base_feature##_t = decltype(&base_feature); \
  inline constexpr flags_ui::FeatureEntry::FeatureParam        \
      base_feature##Variation[] = {                            \
          {kIPHDemoModeFeatureChoiceParam, feature_name}}

// Defines a single flags_ui::FeatureEntry::FeatureVariation entry, fully
// enclosed. This is intended to be used with the declaration of
// |kIPHDemoModeChoiceVariations| below.
#define VARIATION_ENTRY(base_feature) \
  {base_feature##Variation[0].param_value, base_feature##Variation, nullptr}

// Defines a flags_ui::FeatureEntry::FeatureParam for each feature.
DEFINE_VARIATION_PARAM(kIPHDummyFeature, "IPH_Dummy");

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
DEFINE_VARIATION_PARAM(kEsbDownloadRowPromoFeature, "EsbDownloadRowPromo");
#endif
DEFINE_VARIATION_PARAM(kIPHBatterySaverModeFeature, "IPH_BatterySaverMode");
DEFINE_VARIATION_PARAM(kIPHCompanionSidePanelFeature, "IPH_CompanionSidePanel");
DEFINE_VARIATION_PARAM(kIPHCompanionSidePanelRegionSearchFeature,
                       "IPH_CompanionSidePanelRegionSearch");
DEFINE_VARIATION_PARAM(kIPHComposeNewBadgeFeature,
                       "IPH_ComposeNewBadgeFeature");
DEFINE_VARIATION_PARAM(kIPHComposeMSBBSettingsFeature,
                       "IPH_ComposeMSBBSettingsFeature");
DEFINE_VARIATION_PARAM(kIPHDesktopCustomizeChromeAutoOpenFeature,
                       "IPH_DesktopCustomizeChromeAutoOpen");
DEFINE_VARIATION_PARAM(kIPHDesktopCustomizeChromeExperimentFeature,
                       "IPH_DesktopCustomizeChromeExperiment");
DEFINE_VARIATION_PARAM(kIPHDesktopRealboxContextualSearchFeature,
                       "IPH_DesktopRealboxContextualSearchFeature");
DEFINE_VARIATION_PARAM(kIPHDiscardRingFeature, "IPH_DiscardRing");
DEFINE_VARIATION_PARAM(kIPHDownloadEsbPromoFeature, "IPH_DownloadEsbPromo");
DEFINE_VARIATION_PARAM(kIPHExplicitBrowserSigninPreferenceRememberedFeature,
                       "IPH_ExplicitBrowserSigninPreferenceRemembered");
DEFINE_VARIATION_PARAM(kIPHHistorySearchFeature, "IPH_HistorySearch");
#if BUILDFLAG(ENABLE_EXTENSIONS)
DEFINE_VARIATION_PARAM(kIPHExtensionsMenuFeature, "IPH_ExtensionsMenu");
DEFINE_VARIATION_PARAM(kIPHExtensionsRequestAccessButtonFeature,
                       "IPH_ExtensionsRequestAccessButton");
DEFINE_VARIATION_PARAM(kIPHExtensionsZeroStatePromoFeature,
                       "IPH_ExtensionsZeroStatePromo");
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
DEFINE_VARIATION_PARAM(kIPHExtensionsPinnedByDefaultFeature,
                       "IPH_ExtensionsPinnedByDefault");
#endif
DEFINE_VARIATION_PARAM(kIPHGMCCastStartStopFeature, "IPH_GMCCastStartStop");
DEFINE_VARIATION_PARAM(kIPHGMCLocalMediaCastingFeature,
                       "IPH_GMCLocalMediaCasting");
// The feature is used in Finch experiments so it is unable to be renamed
// alongside the variable name.
DEFINE_VARIATION_PARAM(kIPHMemorySaverModeFeature, "IPH_HighEfficiencyMode");
DEFINE_VARIATION_PARAM(kIPHMultistepFilterPromoFeature,
                       "IPH_MultistepFilterPromo");
DEFINE_VARIATION_PARAM(kIPHLensOverlayFeature, "IPH_LensOverlay");
DEFINE_VARIATION_PARAM(kIPHLensOverlayTranslateButtonFeature,
                       "IPH_LensOverlayTranslateButton");
DEFINE_VARIATION_PARAM(kIPHPasswordsSavePrimingPromoFeature,
                       "IPH_PasswordsSavePrimingPromo");
DEFINE_VARIATION_PARAM(kIPHPasswordsSaveRecoveryPromoFeature,
                       "IPH_PasswordsSaveRecoveryPromo");
DEFINE_VARIATION_PARAM(kIPHPasswordsManagementBubbleAfterSaveFeature,
                       "IPH_PasswordsManagementBubbleAfterSave");
DEFINE_VARIATION_PARAM(kIPHPasswordsManagementBubbleDuringSigninFeature,
                       "IPH_PasswordsManagementBubbleDuringSignin");
DEFINE_VARIATION_PARAM(kIPHPasswordsWebAppProfileSwitchFeature,
                       "IPH_PasswordsWebAppProfileSwitch");
DEFINE_VARIATION_PARAM(kIPHPasswordManagerShortcutFeature,
                       "IPH_PasswordManagerShortcut");
DEFINE_VARIATION_PARAM(kIPHPasswordSharingFeature,
                       "IPH_PasswordSharingFeature");
DEFINE_VARIATION_PARAM(kIPHPdfInkSignaturesFeature, "IPH_PdfInkSignatures");
DEFINE_VARIATION_PARAM(kIPHPdfSearchifyFeature, "IPH_PdfSearchifyFeature");
DEFINE_VARIATION_PARAM(kIPHPdfTextAnnotationsFeature, "IPH_PdfTextAnnotations");
DEFINE_VARIATION_PARAM(kIPHPerformanceInterventionDialogFeature,
                       "IPH_PerformanceInterventionDialogFeature");
DEFINE_VARIATION_PARAM(kIPHPowerBookmarksSidePanelFeature,
                       "IPH_PowerBookmarksSidePanel");
DEFINE_VARIATION_PARAM(kIPHPriceInsightsPageActionIconLabelFeature,
                       "IPH_PriceInsightsPageActionIconLabelFeature");
DEFINE_VARIATION_PARAM(kIPHPriceTrackingEmailConsentFeature,
                       "IPH_PriceTrackingEmailConsentFeature");
DEFINE_VARIATION_PARAM(kIPHPriceTrackingPageActionIconLabelFeature,
                       "IPH_PriceTrackingPageActionIconLabelFeature");
DEFINE_VARIATION_PARAM(kIPHReadingListDiscoveryFeature,
                       "IPH_ReadingListDiscovery");
DEFINE_VARIATION_PARAM(kIPHReadingListEntryPointFeature,
                       "IPH_ReadingListEntryPoint");
DEFINE_VARIATION_PARAM(kIPHReadingListInSidePanelFeature,
                       "IPH_ReadingListInSidePanel");
DEFINE_VARIATION_PARAM(kIPHReadingModeKeyboardShortcutFeature,
                       "IPH_ReadingModeKeyboardShortcut");
DEFINE_VARIATION_PARAM(kIPHReadingModePageActionLabelFeature,
                       "IPH_ReadingModePageActionLabel");
DEFINE_VARIATION_PARAM(kIPHReadingModePresentationModeFeature,
                       "IPH_ReadingModePresentationMode");
DEFINE_VARIATION_PARAM(kIPHReadingModeSidePanelFeature,
                       "IPH_ReadingModeSidePanel");
DEFINE_VARIATION_PARAM(kIPHShoppingCollectionFeature,
                       "IPH_ShoppingCollectionFeature");
DEFINE_VARIATION_PARAM(kIPHSideBySidePinnableFeature,
                       "IPH_SideBySidePinnableFeature");
DEFINE_VARIATION_PARAM(kIPHSideBySideTabSwitchFeature,
                       "IPH_SideBySideTabSwitchFeature");
DEFINE_VARIATION_PARAM(kIPHSidePanelContextualTasksPinnableFeature,
                       "IPH_SidePanelContextualTasksPinnableFeature");
DEFINE_VARIATION_PARAM(kIPHSidePanelGenericPinnableFeature,
                       "IPH_SidePanelGenericPinnableFeature");
DEFINE_VARIATION_PARAM(kIPHSidePanelLensOverlayPinnableFeature,
                       "IPH_SidePanelLensOverlayPinnableFeature");
DEFINE_VARIATION_PARAM(kIPHSideSearchAutoTriggeringFeature,
                       "IPH_SideSearchAutoTriggering");
DEFINE_VARIATION_PARAM(kIPHSideSearchPageActionLabelFeature,
                       "IPH_SideSearchPageActionLabel");

DEFINE_VARIATION_PARAM(kIPHSmartTabSharingDefaultOnFeature,
                       "IPH_SmartTabSharingDefaultOnFeature");
DEFINE_VARIATION_PARAM(kIPHSmartTabSharingFeature,
                       "IPH_SmartTabSharingFeature");
DEFINE_VARIATION_PARAM(kIPHSmartTabSharingTryItFeature,
                       "IPH_SmartTabSharingTryItFeature");

DEFINE_VARIATION_PARAM(kIPHSplitViewHorizontalIndirectAccessFeature,
                       "IPH_SplitViewHorizontalIndirectAccessFeature");
DEFINE_VARIATION_PARAM(kIPHVerticalTabsExpandOnHoverFeature,
                       "IPH_VerticalTabsExpandOnHoverFeature");
DEFINE_VARIATION_PARAM(kIPHVerticalTabstripTutorialFeature,
                       "IPH_VerticalTabstripTutorialFeature");

DEFINE_VARIATION_PARAM(kIPHPwaQuietNotificationFeature,
                       "IPH_PwaQuietNotification");
DEFINE_VARIATION_PARAM(kIPHTabAudioMutingFeature, "IPH_TabAudioMuting");
DEFINE_VARIATION_PARAM(kIPHTabSearchComboButtonFeature,
                       "IPH_TabSearchComboButton");
DEFINE_VARIATION_PARAM(kIPHDesktopPwaInstallFeature, "IPH_DesktopPwaInstall");
DEFINE_VARIATION_PARAM(kIPHProfileSwitchFeature, "IPH_ProfileSwitch");
DEFINE_VARIATION_PARAM(kIPHDesktopSharedHighlightingFeature,
                       "IPH_DesktopSharedHighlighting");
DEFINE_VARIATION_PARAM(kIPHWebUiHelpBubbleTestFeature,
                       "IPH_WebUiHelpBubbleTest");
DEFINE_VARIATION_PARAM(kIPHPriceTrackingInSidePanelFeature,
                       "IPH_PriceTrackingInSidePanel");
DEFINE_VARIATION_PARAM(kIPHBackNavigationMenuFeature, "IPH_BackNavigationMenu");
DEFINE_VARIATION_PARAM(kIPHTabGroupsSaveV2IntroFeature,
                       "IPH_TabGroupsSaveV2Intro");
DEFINE_VARIATION_PARAM(kIPHTabGroupsSaveV2CloseGroupFeature,
                       "IPH_TabGroupsSaveV2CloseGroup");
DEFINE_VARIATION_PARAM(kIPHTabGroupsSharedTabChangedFeature,
                       "IPH_TabGroupsSharedTabChanged");
DEFINE_VARIATION_PARAM(kIPHTabGroupsSharedTabFeedbackFeature,
                       "IPH_TabGroupsSharedTabFeedback");
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)
DEFINE_VARIATION_PARAM(kIPHAutofillAccountNameEmailSuggestionFeature,
                       "IPH_AutofillAccountNameEmailSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillAiOptInFeature, "IPH_AutofillAiOptIn");
DEFINE_VARIATION_PARAM(kIPHAutofillAiValuablesFeature,
                       "IPH_AutofillAiValuables");
DEFINE_VARIATION_PARAM(kIPHAutofillAtMemoryFeature, "IPH_AutofillAtMemory");
DEFINE_VARIATION_PARAM(kIPHAutofillBnplAffirmOrZipSuggestionFeature,
                       "IPH_AutofillBnplAffirmOrZipSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillBnplAffirmZipOrKlarnaSuggestionFeature,
                       "IPH_AutofillBnplAffirmZipOrKlarnaSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillCreditCardBenefitFeature,
                       "IPH_AutofillCreditCardBenefit");
DEFINE_VARIATION_PARAM(kIPHAutofillCardInfoRetrievalSuggestionFeature,
                       "IPH_AutofillCardInfoRetrievalSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillDisabledVirtualCardSuggestionFeature,
                       "IPH_AutofillDisabledVirtualCardSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillDownstreamCardAwarenessFeature,
                       "IPH_AutofillDownstreamCardAwareness");
DEFINE_VARIATION_PARAM(kIPHAutofillEnableLoyaltyCardsFeature,
                       "IPH_AutofillEnableLoyaltyCards");
DEFINE_VARIATION_PARAM(kIPHAutofillExternalAccountProfileSuggestionFeature,
                       "IPH_AutofillExternalAccountProfileSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillHomeWorkProfileSuggestionFeature,
                       "IPH_AutofillHomeWorkProfileSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillOmniboxPaymentChipFeature,
                       "IPH_AutofillOmniboxPaymentChip");
DEFINE_VARIATION_PARAM(kIPHAutofillVirtualCardCVCSuggestionFeature,
                       "IPH_AutofillVirtualCardCVCSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillVirtualCardSuggestionFeature,
                       "IPH_AutofillVirtualCardSuggestion");

#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
DEFINE_VARIATION_PARAM(kIPHDesktopPWAsLinkCapturingLaunch,
                       "IPH_DesktopPWAsLinkCapturingLaunch");
DEFINE_VARIATION_PARAM(kIPHDesktopPWAsLinkCapturingLaunchAppInTab,
                       "IPH_DesktopPWAsLinkCapturingLaunchAppInTab");
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
DEFINE_VARIATION_PARAM(kIPHSignInBenefitsFeature, "IPH_SignInBenefits");
DEFINE_VARIATION_PARAM(kIPHSupervisedUserProfileSigninFeature,
                       "IPH_SupervisedUserProfileSignin");
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

DEFINE_VARIATION_PARAM(kIPHiOSPasswordPromoDesktopFeature,
                       "IPH_iOSPasswordPromoDesktop");
DEFINE_VARIATION_PARAM(kIPHiOSAddressPromoDesktopFeature,
                       "IPH_iOSAddressPromoDesktop");
DEFINE_VARIATION_PARAM(kIPHiOSPaymentPromoDesktopFeature,
                       "IPH_iOSPaymentPromoDesktop");
DEFINE_VARIATION_PARAM(kIPHiOSLensPromoDesktopFeature,
                       "IPH_iOSLensPromoDesktop");
DEFINE_VARIATION_PARAM(kIPHiOSEnhancedBrowsingDesktopFeature,
                       "IPH_iOSEnhancedBrowsingDesktop");
DEFINE_VARIATION_PARAM(kIPHiOSTabGroupsDesktopFeature,
                       "IPH_iOSTabGroupsDesktop");
DEFINE_VARIATION_PARAM(kIPHiOSPriceTrackingDesktopFeature,
                       "IPH_iOSPriceTrackingDesktop");

DEFINE_VARIATION_PARAM(kIPHResumptionRailFeature, "IPH_ResumptionRail");

// Defines the array of which features should be listed in the chrome://flags
// UI to be able to select them alone for demo-mode. The features listed here
// are possible to enable on their own in demo mode.
inline constexpr flags_ui::FeatureEntry::FeatureVariation
    kIPHDemoModeChoiceVariations[] = {
        // clang-format off
// keep-sorted start case=no
        VARIATION_ENTRY(kIPHiOSAddressPromoDesktopFeature),
        VARIATION_ENTRY(kIPHiOSEnhancedBrowsingDesktopFeature),
        VARIATION_ENTRY(kIPHiOSLensPromoDesktopFeature),
        VARIATION_ENTRY(kIPHiOSPasswordPromoDesktopFeature),
        VARIATION_ENTRY(kIPHiOSPaymentPromoDesktopFeature),
        VARIATION_ENTRY(kIPHiOSPriceTrackingDesktopFeature),
        VARIATION_ENTRY(kIPHiOSTabGroupsDesktopFeature),
// keep-sorted end

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#if BUILDFLAG(ENABLE_EXTENSIONS)
        VARIATION_ENTRY(kIPHExtensionsMenuFeature),
        VARIATION_ENTRY(kIPHExtensionsRequestAccessButtonFeature),
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
        VARIATION_ENTRY(kIPHExtensionsPinnedByDefaultFeature),
#endif
// keep-sorted start case=no
        VARIATION_ENTRY(kIPHBackNavigationMenuFeature),
        VARIATION_ENTRY(kIPHBatterySaverModeFeature),
        VARIATION_ENTRY(kIPHCompanionSidePanelFeature),
        VARIATION_ENTRY(kIPHCompanionSidePanelRegionSearchFeature),
        VARIATION_ENTRY(kIPHComposeMSBBSettingsFeature),
        VARIATION_ENTRY(kIPHComposeNewBadgeFeature),
        VARIATION_ENTRY(kIPHDesktopCustomizeChromeAutoOpenFeature),
        VARIATION_ENTRY(kIPHDesktopCustomizeChromeExperimentFeature),
        VARIATION_ENTRY(kIPHDesktopPwaInstallFeature),
        VARIATION_ENTRY(kIPHDesktopRealboxContextualSearchFeature),
        VARIATION_ENTRY(kIPHDesktopSharedHighlightingFeature),
        VARIATION_ENTRY(kIPHDiscardRingFeature),
        VARIATION_ENTRY(kIPHExplicitBrowserSigninPreferenceRememberedFeature),
        VARIATION_ENTRY(kIPHGMCCastStartStopFeature),
        VARIATION_ENTRY(kIPHGMCLocalMediaCastingFeature),
        VARIATION_ENTRY(kIPHHistorySearchFeature),
        VARIATION_ENTRY(kIPHMemorySaverModeFeature),
        VARIATION_ENTRY(kIPHMultistepFilterPromoFeature),
        VARIATION_ENTRY(kIPHPasswordManagerShortcutFeature),
        VARIATION_ENTRY(kIPHPasswordSharingFeature),
        VARIATION_ENTRY(kIPHPasswordsManagementBubbleAfterSaveFeature),
        VARIATION_ENTRY(kIPHPasswordsManagementBubbleDuringSigninFeature),
        VARIATION_ENTRY(kIPHPasswordsWebAppProfileSwitchFeature),
        VARIATION_ENTRY(kIPHPdfInkSignaturesFeature),
        VARIATION_ENTRY(kIPHPdfSearchifyFeature),
        VARIATION_ENTRY(kIPHPdfTextAnnotationsFeature),
        VARIATION_ENTRY(kIPHPerformanceInterventionDialogFeature),
        VARIATION_ENTRY(kIPHPowerBookmarksSidePanelFeature),
        VARIATION_ENTRY(kIPHPriceInsightsPageActionIconLabelFeature),
        VARIATION_ENTRY(kIPHPriceTrackingEmailConsentFeature),
        VARIATION_ENTRY(kIPHPriceTrackingInSidePanelFeature),
        VARIATION_ENTRY(kIPHPriceTrackingPageActionIconLabelFeature),
        VARIATION_ENTRY(kIPHProfileSwitchFeature),
        VARIATION_ENTRY(kIPHPwaQuietNotificationFeature),
        VARIATION_ENTRY(kIPHReadingListDiscoveryFeature),
        VARIATION_ENTRY(kIPHReadingListEntryPointFeature),
        VARIATION_ENTRY(kIPHReadingListInSidePanelFeature),
        VARIATION_ENTRY(kIPHReadingModeKeyboardShortcutFeature),
        VARIATION_ENTRY(kIPHReadingModePageActionLabelFeature),
        VARIATION_ENTRY(kIPHReadingModePresentationModeFeature),
        VARIATION_ENTRY(kIPHReadingModeSidePanelFeature),
        VARIATION_ENTRY(kIPHResumptionRailFeature),
        VARIATION_ENTRY(kIPHShoppingCollectionFeature),
        VARIATION_ENTRY(kIPHSideBySidePinnableFeature),
        VARIATION_ENTRY(kIPHSideBySideTabSwitchFeature),
        VARIATION_ENTRY(kIPHSidePanelContextualTasksPinnableFeature),
        VARIATION_ENTRY(kIPHSidePanelGenericPinnableFeature),
        VARIATION_ENTRY(kIPHSideSearchAutoTriggeringFeature),
        VARIATION_ENTRY(kIPHSideSearchPageActionLabelFeature),
        VARIATION_ENTRY(kIPHSmartTabSharingDefaultOnFeature),
        VARIATION_ENTRY(kIPHSmartTabSharingFeature),
        VARIATION_ENTRY(kIPHSmartTabSharingTryItFeature),
        VARIATION_ENTRY(kIPHSplitViewHorizontalIndirectAccessFeature),
        VARIATION_ENTRY(kIPHTabAudioMutingFeature),
        VARIATION_ENTRY(kIPHTabGroupsSharedTabChangedFeature),
        VARIATION_ENTRY(kIPHTabGroupsSharedTabFeedbackFeature),
        VARIATION_ENTRY(kIPHTabSearchComboButtonFeature),
        VARIATION_ENTRY(kIPHWebUiHelpBubbleTestFeature),
// keep-sorted end
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)
// keep-sorted start case=no
        VARIATION_ENTRY(kIPHAutofillAccountNameEmailSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillAiOptInFeature),
        VARIATION_ENTRY(kIPHAutofillAiValuablesFeature),
        VARIATION_ENTRY(kIPHAutofillAtMemoryFeature),
        VARIATION_ENTRY(kIPHAutofillCardInfoRetrievalSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillCreditCardBenefitFeature),
        VARIATION_ENTRY(kIPHAutofillDisabledVirtualCardSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillDownstreamCardAwarenessFeature),
        VARIATION_ENTRY(kIPHAutofillEnableLoyaltyCardsFeature),
        VARIATION_ENTRY(kIPHAutofillExternalAccountProfileSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillHomeWorkProfileSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillOmniboxPaymentChipFeature),
        VARIATION_ENTRY(kIPHAutofillVirtualCardCVCSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillVirtualCardSuggestionFeature),

// keep-sorted end
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        VARIATION_ENTRY(kIPHDesktopPWAsLinkCapturingLaunch),
        VARIATION_ENTRY(kIPHDesktopPWAsLinkCapturingLaunchAppInTab),
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        VARIATION_ENTRY(kIPHSignInBenefitsFeature),
        VARIATION_ENTRY(kIPHSupervisedUserProfileSigninFeature),
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

        // clang-format on
};

#undef DEFINE_VARIATION_PARAM
#undef VARIATION_ENTRY

// Returns all the features that are in use for engagement tracking.
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
FeatureVector GetAllFeatures();

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
