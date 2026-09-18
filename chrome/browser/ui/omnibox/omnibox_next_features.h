// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NEXT_FEATURES_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NEXT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "third_party/omnibox_proto/ntp_composebox_config.pb.h"

class Profile;

namespace omnibox {

namespace internal {
// The `internal` namespace contains implementation details for omnibox
// features. It is exposed only for use by about_flags.cc and in unit tests.
//
// DO NOT USE THESE FEATURE FLAGS DIRECTLY.
//
// These flags are placed in `internal` to signify that they often require
// specific initialization sequences
// (e.g., in `chrome/browser/chrome_browser_interface_binders_webui_parts.h`)
// or have lifecycle-sensitive dependencies that could lead to crashes if
// checked improperly.
//
// USE THE APPROPRIATE HELPER:
// - Use the feature-specific `...FeatureEnabled()` function when you only
//   need to check the raw feature state (appropriate for lifecycle-sensitive code).
// - Use the profile-based `...Enabled(profile)` function for standard UI
//   logic (e.g., `IsAimPopupEnabled(profile)`), as it handles necessary
//   eligibility and initialization checks.
//
// If you are adding a new feature that does not have these complex lifecycle
// dependencies, you may define it in the main `omnibox` namespace.

BASE_DECLARE_FEATURE(kWebUIOmniboxPopup);
// TODO(crbug.com/521521553): Remove this feature flag once the
// feature flag is fully rolled out.
BASE_DECLARE_FEATURE(kWebUIOmniboxSimplification);

}  // namespace internal

extern const base::FeatureParam<bool> kHideClassicContextButton;
BASE_DECLARE_FEATURE(kWebUIOmniboxDisableCaretColorAnimation);
BASE_DECLARE_FEATURE(kWebUIOmniboxFullPopup);
BASE_DECLARE_FEATURE(kWebUIOmniboxFullPopupDoubleClick);
BASE_DECLARE_FEATURE(kOmniboxEverywhere);
BASE_DECLARE_FEATURE(kWebUIOmniboxPopupDebug);
BASE_DECLARE_FEATURE(kWebUIOmniboxPopupSelectionControl);
// Caret animation for omnibox
BASE_DECLARE_FEATURE(kOmniboxAnimatedCaret);
// Enables energy effect in the omnibox.
BASE_DECLARE_FEATURE(kEnergyEffectInOmnibox);
BASE_DECLARE_FEATURE(kWebUIOmniboxDynamicAiModeButton);
// Prevents closing popup while file chooser is open.
BASE_DECLARE_FEATURE(kOmniboxKeepOpenOnFileSelection);

extern const base::FeatureParam<bool> kOmniboxEverywhereProfilePickerParam;

// Controls showing most visited tiles in OmniboxEverywhere.
extern const base::FeatureParam<bool> kOmniboxEverywhereMostVisitedParam;
// Whether to use the grey oblong background for context menu entrypoint.
extern const base::FeatureParam<bool> kContextButtonHasBackground;
// Whether the button should be an oblong shape vs circular.
extern const base::FeatureParam<bool> kContextButtonShapeIsOblong;
// Whether to show the "Ask about tabs" label for the context menu entrypoint.
extern const base::FeatureParam<bool> kContextButtonShowSuggestionLabel;
// If enabled, then the WebUI Omnibox will be rendered in a WebView in the
// BrowserView.
extern const base::FeatureParam<bool> kWebUIOmniboxFullPopupUseBrowserView;
extern const base::FeatureParam<bool> kWebUIOmniboxFullPopupMultiline;
// Whether to enable dynamic animation for the WebUI Omnibox.
extern const base::FeatureParam<bool> kWebUIOmniboxDynamicAnimation;
// Whether to enable dynamic color scheme for the WebUI Omnibox.
extern const base::FeatureParam<bool> kWebUIOmniboxDynamicColorScheme;

// Returns true if `kWebUIOmniboxPopup` is enabled.
bool IsWebUIOmniboxPopupEnabled();

// Returns true if `kWebUIOmniboxFullPopup` is enabled.
bool IsWebUIOmniboxFullPopupEnabled();

// Returns true if the webui omnibox should use the WebuiOmniboxFullHandler
bool ShouldUseWebUIOmniboxFullHandler();

// Returns true if `kWebUIOmniboxInBrowserView` is enabled.
bool IsWebUIOmniboxInBrowserViewEnabled();

// Returns true if the Omnibox Everywhere feature is fully enabled for the given
// `profile`. This checks both the base::Feature flag and that Google is the
// default search provider.
bool IsOmniboxEverywhereEnabled(Profile* profile);

}  // namespace omnibox

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NEXT_FEATURES_H_
