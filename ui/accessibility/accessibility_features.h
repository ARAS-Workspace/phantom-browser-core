// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the base::Features used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_base_export.h"

// This file declares base::Features related to the ui/accessibility code.
//
// If your flag is for all platforms, include it in the first section. If
// the flag is build-specific, include it in the appropriate section or
// add a new section if needed.
//
// Keep all sections ordered alphabetically.
//
// Include the base declaration, and a bool "is...Enabled()" getter for
// convenience and consistency. The bool method does not need a comment. Place a
// comment above the feature flag describing what the flag does when enabled.
// For example, a new entry should look like:
//
//    // <<effect of the experiment>>
//    AX_BASE_EXPORT BASE_DECLARE_FEATURE(kNewFeature);
//    AX_BASE_EXPORT bool IsNewFeatureEnabled();
//
// In the .cc file, a corresponding new entry should look like:
//
//    BASE_FEATURE(kNewFeature, base::FEATURE_DISABLED_BY_DEFAULT);
//    bool IsNewFeatureEnabled() {
//      return base::FeatureList::IsEnabled(::features::kNewFeature);
//    }
//
// Your feature name should start with "kAccessibility". There is no need to
// include the words "enabled" or "experimental", as these are implied. We
// include accessibility to differentiate these features from others in
// Chromium.

namespace features {

// Enable PDF OCR for Select-to-Speak. It will be disabled by default on
// platforms other than ChromeOS as STS is available only on ChromeOS.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityPdfOcrForSelectToSpeak);
AX_BASE_EXPORT bool IsAccessibilityPdfOcrForSelectToSpeakEnabled();

// A replacement algorithm for AbstractInlineTextBox + InlineCursor for
// AXInlineTextBox creation.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityBlockFlowIterator);
AX_BASE_EXPORT bool IsAccessibilityBlockFlowIteratorEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilityPruneRedundantInlineConnectivity);
AX_BASE_EXPORT bool IsAccessibilityPruneRedundantInlineConnectivityEnabled();

// Enables the addition of text formatting information to the Android
// AccessibilityNodeInfo accessibility tree.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityTextFormatting);
AX_BASE_EXPORT bool IsAccessibilityTextFormattingEnabled();

// Enables handling of occluding views belonging to overlaid "
// "browser's UI.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityHandleOccludingViews);
AX_BASE_EXPORT bool IsAccessibilityHandleOccludingViewsEnabled();

// Enables text change types for text changed events.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityTextChangeTypes);
AX_BASE_EXPORT bool IsAccessibilityTextChangeTypesEnabled();

// Enables the addition of `labeledby` relationships in the accessibility tree.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityLabeledBy);
AX_BASE_EXPORT bool IsAccessibilityLabeledByEnabled();

// Expose the accessibility tree for views via an AXTree of AXNodes.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityTreeForViews);
AX_BASE_EXPORT bool IsAccessibilityTreeForViewsEnabled();

// Serialize Views' accessibility data as soon as it changes.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kViewsAccessibilitySerializeOnDataChange);
AX_BASE_EXPORT bool IsViewsAccessibilitySerializeOnDataChangeEnabled();

// Experiment to measure the performance impact of various accessibility
// changes.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilityPerformanceMeasurementExperiment);
AX_BASE_EXPORT bool IsAccessibilityPerformanceMeasurementExperimentEnabled();

enum class AccessibilityPerformanceMeasurementExperimentGroup {
  kAXModeComplete,
  kWebContentsOnly,
  kAXModeCompleteNoInlineTextBoxes,
  kRendererSerializationOnly,
};

AX_BASE_EXPORT AccessibilityPerformanceMeasurementExperimentGroup
GetAccessibilityPerformanceMeasurementExperimentGroup();

// Use Alternative mechanism for acquiring image descriptions.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kImageDescriptionsAlternateRouting);
AX_BASE_EXPORT bool IsImageDescriptionsAlternateRoutingEnabled();



// Expose <summary>" as a heading instead of a button.
// Two reasons to try this:
// 1. Unlike for a button, JAWS will not enforce leafiness for a heading, so
// that things like child links will still be presented to the user.
// 2. The user can use heading navigation for summaries.
// We may decide to scale this back for use cases such as a summary inside of
// a table or a list.
// Experiment until we validate the approach with ATs and ARIA WG.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityExposeSummaryAsHeading);
AX_BASE_EXPORT bool IsAccessibilityExposeSummaryAsHeadingEnabled();

// Extension manifest v3 migration for network speech synthesis.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kExtensionManifestV3NetworkSpeechSynthesis);
AX_BASE_EXPORT bool IsExtensionManifestV3NetworkSpeechSynthesisEnabled();

// Expose document markers on inline text boxes in addition to
// static nodes. (Note: This will make it possible for AXPosition in the browser
// process to handle document markers, which will be platform agnositc)
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kUseAXPositionForDocumentMarkers);
AX_BASE_EXPORT bool IsUseAXPositionForDocumentMarkersEnabled();

// Randomly turn the accessibility engine on based on certain conditions. We do
// not put this flag in chrome://flags so we can get the cleanest data possible.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAXRandomizedStressTests);
AX_BASE_EXPORT bool IsAXRandomizedStressTestsEnabled();

// Enable the experimental on-screen AXMode .
// TODO(accessibility): Only turn on the experimental On-Screen mode for when
// screen readers are not running. This is an experimental mode for now, so this
// is fine for now.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityOnScreenMode);

// Returns true if the on screen AXMode is enabled.
AX_BASE_EXPORT bool IsAccessibilityOnScreenAXModeEnabled();

// Controls canvas accessibility mode.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityCanvas);

enum class CanvasAccessibilityMode {
  kDisabled,
  kBasic,     // Only rendered text.
  kAdvanced,  // Rendered text + OCR.
};

AX_BASE_EXPORT CanvasAccessibilityMode GetCanvasAccessibilityMode();

// Controls whether canvas accessibility heuristic results are collected via
// UKM.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kEnableCollectAccessibilityHeuristicInCanvasUkm);
AX_BASE_EXPORT bool IsCollectAccessibilityHeuristicInCanvasUkmEnabled();



// Use the AXTree fixing code, which may be an assortment of different
// tools/methods to fix the AXTree. This is not available on Android.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAXTreeFixing);
AX_BASE_EXPORT bool IsAXTreeFixingEnabled();

// Identify and annotate the main node of the AXTree where one was not already
// provided.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kMainNodeAnnotations);
AX_BASE_EXPORT bool IsMainNodeAnnotationsEnabled();

// Enable heuristic enhancements for PDF accessibility.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kPdfAccessibilityHeuristicEnhancements);
AX_BASE_EXPORT bool IsPdfAccessibilityHeuristicEnhancementsEnabled();

// ScreenAI library's Main Content Extraction service is enabled.
AX_BASE_EXPORT bool IsScreenAIMainContentExtractionEnabled();

// ScreenAI library's OCR service is enabled.
AX_BASE_EXPORT bool IsScreenAIOCREnabled();

// Enables to use the Screen AI component available for testing.
// If enabled, ScreenAI library will be loaded from //third_party/screen-ai.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kScreenAITestMode);
AX_BASE_EXPORT bool IsScreenAITestModeEnabled();

#if BUILDFLAG(IS_LINUX)
// Enables advanced partition allocation checks in ScreenAI service.
// TODO(crbug.com/418199684): Remove when the bug is fixed.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kScreenAIPartitionAllocAdvancedChecksEnabled);
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC)
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kMacAccessibilityAPIMigration);
AX_BASE_EXPORT bool IsMacAccessibilityAPIMigrationEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kMacAccessibilityOptimizeChildrenChanged);
AX_BASE_EXPORT bool IsMacAccessibilityOptimizeChildrenChangedEnabled();

// Set NSAccessibilityRemoteUIElement's RemoteUIApp to YES to fix
// some accessibility bugs in PWA Mac. (Note: When enabling
// NSAccessibilityRemoteUIElement's RemoteUIApp previously, chromium would hang.
// See: https://crbug.com/1491329).
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityRemoteUIApp);
AX_BASE_EXPORT bool IsAccessibilityRemoteUIAppEnabled();

// TODO (crbug.com/380927771). Remove this flag once VoiceOver has incorporated
// the name change event.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kBlockRootWindowAccessibleNameChangeEvent);
AX_BASE_EXPORT bool IsBlockRootWindowAccessibleNameChangeEventEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kMacAccessibilityTextOperation);
AX_BASE_EXPORT bool IsMacAccessibilityTextOperationEnabled();
#endif  // BUILDFLAG(IS_MAC)

}  // namespace features

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
