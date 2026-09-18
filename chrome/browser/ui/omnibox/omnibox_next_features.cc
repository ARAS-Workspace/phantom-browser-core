// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_next_features.h"

#include <string>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/search.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr base::FeatureState DISABLED = base::FEATURE_DISABLED_BY_DEFAULT;
constexpr base::FeatureState ENABLED = base::FEATURE_ENABLED_BY_DEFAULT;
}  // namespace

namespace omnibox {

namespace internal {

// If enabled, shows the omnibox suggestions in the popup in WebUI.
BASE_FEATURE(kWebUIOmniboxPopup, ENABLED);

// If enabled, the Omnibox Popup will enable a different UI state when on a
// webpage.
BASE_FEATURE(kWebUIOmniboxSimplification, DISABLED);

}  // namespace internal

// If true, hides the "Add Context" button in the "classic" popup.
const base::FeatureParam<bool> kHideClassicContextButton{
    &internal::kWebUIOmniboxSimplification, "Omnibox_HideClassicContextButton",
    true};

// If enabled, disables caret color animation for the WebUI Omnibox AIM popup.
BASE_FEATURE(kWebUIOmniboxDisableCaretColorAnimation, ENABLED);
// If enabled, then both the input row and suggestions dropdown (in the Omnibox)
// will be rendered using the WebUI stack (i.e. the cutout for the location bar
// will be removed).
BASE_FEATURE(kWebUIOmniboxFullPopup, DISABLED);
// Enables the double click mechanism of sending selection set by
// passing click events through the WebView.
BASE_FEATURE(kWebUIOmniboxFullPopupDoubleClick, ENABLED);
// If enabled, enables OmniboxEverywhere popup triggered by shortcut.
BASE_FEATURE(kOmniboxEverywhere, DISABLED);
// Controls showing the profile picker menu on profile avatar click in
// OmniboxEverywhere.
const base::FeatureParam<bool> kOmniboxEverywhereProfilePickerParam{
    &kOmniboxEverywhere, "ProfilePicker", false};
// Controls showing most visited tiles in OmniboxEverywhere.
const base::FeatureParam<bool> kOmniboxEverywhereMostVisitedParam{
    &kOmniboxEverywhere, "MostVisited", true};
// Enables the WebUI for omnibox suggestions without modifying the popup UI.
BASE_FEATURE(kWebUIOmniboxPopupDebug, DISABLED);
// If enabled, the WebUIOmniboxPopup controls its own selection state instead of
// following that of the OmniboxEditModel.
BASE_FEATURE(kWebUIOmniboxPopupSelectionControl, DISABLED);

// If enabled, animates the caret in the omnibox.
BASE_FEATURE(kOmniboxAnimatedCaret, ENABLED);

// If enabled, enables energy effect in the omnibox.
BASE_FEATURE(kEnergyEffectInOmnibox, ENABLED);

// If enabled, the Ai Mode button will be dynamically shown in the omnibox.
BASE_FEATURE(kWebUIOmniboxDynamicAiModeButton, DISABLED);

// If enabled, prevents closing the AIM popup while file chooser is open.
// Disabled due to focus restoration and popup deactivation issues.
BASE_FEATURE(kOmniboxKeepOpenOnFileSelection, DISABLED);

// Decodes a proto object from its serialized Base64 string representation.
// Returns true if decoding and parsing succeed, false otherwise.
bool ParseProtoFromBase64String(const std::string& input,
                                google::protobuf::MessageLite& output) {
  if (input.empty()) {
    return false;
  }

  std::string decoded_input;
  // Decode the Base64-encoded input string into decoded_input.
  if (!base::Base64Decode(input, &decoded_input)) {
    return false;
  }

  if (decoded_input.empty()) {
    return false;
  }

  // Parse the decoded string into the proto object.
  return output.ParseFromString(decoded_input);
}

bool IsWebUIOmniboxPopupEnabled() {
  return base::FeatureList::IsEnabled(internal::kWebUIOmniboxPopup);
}

bool IsWebUIOmniboxFullPopupEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup);
}

bool ShouldUseWebUIOmniboxFullHandler() {
  return IsWebUIOmniboxFullPopupEnabled() &&
         base::FeatureList::IsEnabled(
             omnibox::kWebUISearchboxWithoutModelController);
}

bool IsWebUIOmniboxInBrowserViewEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup) &&
         kWebUIOmniboxFullPopupUseBrowserView.Get();
}

bool IsOmniboxEverywhereEnabled(Profile* profile) {
  if (!profile) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(kOmniboxEverywhere)) {
    return false;
  }

  return search::DefaultSearchProviderIsGoogle(
      TemplateURLServiceFactory::GetForProfile(profile));
}

const base::FeatureParam<bool> kContextButtonHasBackground{
    &internal::kWebUIOmniboxSimplification,
    "Omnibox_ContextButtonHasBackground", false};
const base::FeatureParam<bool> kContextButtonShapeIsOblong{
    &internal::kWebUIOmniboxSimplification,
    "Omnibox_ContextButtonShapeIsOblong", false};
const base::FeatureParam<bool> kContextButtonShowSuggestionLabel{
    &internal::kWebUIOmniboxSimplification,
    "Omnibox_ContextButtonShowSuggestionLabel", false};
const base::FeatureParam<bool> kWebUIOmniboxFullPopupUseBrowserView{
    &kWebUIOmniboxFullPopup, "Omnibox_UseBrowserView", false};
const base::FeatureParam<bool> kWebUIOmniboxFullPopupMultiline{
    &kWebUIOmniboxFullPopup, "Omnibox_Multiline", false};
const base::FeatureParam<bool> kWebUIOmniboxDynamicAnimation{
    &kWebUIOmniboxDynamicAiModeButton, "Omnibox_DynamicAnimation", false};
const base::FeatureParam<bool> kWebUIOmniboxDynamicColorScheme{
    &kWebUIOmniboxDynamicAiModeButton, "Omnibox_DynamicColorScheme", false};

}  // namespace omnibox
