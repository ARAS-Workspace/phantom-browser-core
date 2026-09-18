// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_interactive_test_mixin.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kClassicPopupWebView);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kDropdownContent = {"omnibox-popup-app",
                                    "cr-searchbox-dropdown", "#content"};
const DeepQuery kClassicMatch = {"omnibox-popup-app", "cr-searchbox-dropdown",
                                 "cr-searchbox-match"};
const DeepQuery kClassicMatchText = {"omnibox-popup-app",
                                     "cr-searchbox-dropdown",
                                     "cr-searchbox-match", "#suggestion"};

}  // namespace

class OmniboxWebUiInteractiveTestBase
    : public SearchboxInteractiveTestMixin<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  OmniboxWebUiInteractiveTestBase() = default;
  ~OmniboxWebUiInteractiveTestBase() override = default;

 protected:
  static std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    std::vector<base::test::FeatureRefAndParams> features = {
        {omnibox::internal::kWebUIOmniboxPopup, {}},
        {omnibox::kOmniboxWebUIDeferShowUntilVisualStateReady, {}}};
    return features;
  }

  // Returns the currently visible `OmniboxPopupWebUIContent`. An
  // `OmniboxPopupView` may host multiple content views, but only one is
  // visible at any given time.
  auto GetActiveClassicPopupWebView() {
    return base::BindLambdaForTesting([&]() -> views::View* {
      auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->toolbar()
              ->location_bar_view()
              ->GetOmniboxPopupView());
      return popup_view->presenter()->GetWebUIContent();
    });
  }

  auto WaitForClassicPopupReady() {
    return Steps(
        InAnyContext(
            WaitForShow(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
        InAnyContext(InstrumentNonTabWebView(kClassicPopupWebView,
                                             GetActiveClassicPopupWebView())),
        InSameContext(WaitForWebContentsReady(
            kClassicPopupWebView, GURL(chrome::kChromeUIOmniboxPopupURL))));
  }
};

class OmniboxWebUiInteractiveTest : public OmniboxWebUiInteractiveTestBase {
 public:
  OmniboxWebUiInteractiveTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetEnabledFeatures(), {});
  }

 protected:
  // Enters Gemini mode in the omnibox and waits for the popup to be ready.
  auto EnterGeminiMode() {
    return Steps(FocusElement(kOmniboxElementId),
                 EnterText(kOmniboxElementId, u"@gemini"),
                 SendKeyPress(kOmniboxElementId, ui::VKEY_TAB),
                 WaitForClassicPopupReady());
  }

  auto WaitForElementToHide(const ui::ElementIdentifier& contents_id,
                            const DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementHides);
    StateChange element_hides;
    element_hides.event = kElementHides;
    element_hides.where = element;
    element_hides.test_function =
        "(el) => { let rect = el.getBoundingClientRect(); return rect.width "
        "=== 0 && rect.height === 0; }";
    return WaitForStateChange(contents_id, element_hides);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Ensures dropdown resurfaces if it goes away during an Omnibox session.
IN_PROC_BROWSER_TEST_F(OmniboxWebUiInteractiveTest, PopupResurfaces) {
  RunTestSequence(
      // Enter Gemini mode in Omnibox.
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      EnterGeminiMode(),
      // With a query entered, no matches should show.
      EnterText(kOmniboxElementId, u"q"),
      InAnyContext(
          WaitForElementToHide(kClassicPopupWebView, kDropdownContent)),
      // Pressing backspace should surface matches.
      SendKeyPress(kOmniboxElementId, ui::VKEY_BACK),
      InAnyContext(
          WaitForElementToRender(kClassicPopupWebView, kClassicMatchText)));
}

// Ensures matches show in Gemini mode when there is input, and that
// pressing enter still navigates to Gemini.
IN_PROC_BROWSER_TEST_F(OmniboxWebUiInteractiveTest, GeminiHidesVerbatimMatch) {
  RunTestSequence(
      // Enter Gemini mode in Omnibox.
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      EnterGeminiMode(),
      // With a query entered, no suggestion match should be shown.
      EnterText(kOmniboxElementId, u"query"),
      InAnyContext(
          WaitForElementToHide(kClassicPopupWebView, kDropdownContent)),
      // Confirming should navigate to the Gemini URL.
      Confirm(kOmniboxElementId),
      WaitForWebContentsNavigation(
          kNewTab, GURL(OmniboxFieldTrial::kGeminiUrlOverride.Get())));
}

// Ensures Gemini mode's null match; e.g. "<Type search term>" is hidden, and
// that clicking the default search suggestion navigates correctly.
// TODO(crbug.com/496926191): Re-enable after de-flaking.
IN_PROC_BROWSER_TEST_F(OmniboxWebUiInteractiveTest, GeminiHidesNullMatch) {
  RunTestSequence(
      // Enter Gemini mode in Omnibox.
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      EnterGeminiMode(),
      // Ensure the initial match is the default search suggestion.
      WaitForVerbatimMatch(kClassicPopupWebView, kClassicMatchText, "@gemini"),
      // Clicking the top match should navigate to a Google search results page.
      InSameContext(ClickElement(kClassicPopupWebView, kClassicMatch)),
      WaitForGoogleSearch(kNewTab, {{"q", "@gemini"}, {"oq", "@gemini"}}));
}

// TODO(crbug.com/496926191): Interactive tests involving verbatim matches are
// fickle, especially in the WebUI Omnibox popup. Since we have to wait for the
// `SetPage` call in the `SearchboxHandler` to complete before the popup can
// receive results, we can't guarantee that this test will always pass.

// class OmniboxSubmitInteractiveTest : public OmniboxWebUiInteractiveTestBase,
//                                      public testing::WithParamInterface<bool>
//                                      {
//  public:
//   OmniboxSubmitInteractiveTest() {
//     feature_list_.InitWithFeaturesAndParameters(
//         GetEnabledFeatures(), {});
//   }

//   bool ClickMatch() const { return GetParam(); }

//  private:
//   base::test::ScopedFeatureList feature_list_;
// };

// INSTANTIATE_TEST_SUITE_P(All, OmniboxSubmitInteractiveTest, testing::Bool());

// IN_PROC_BROWSER_TEST_P(OmniboxSubmitInteractiveTest,
//                        SubmittingInputNavigatesToSearch) {
//   RunTestSequence(
//       // Open the Omnibox with a seeded history result (to avoid flakiness).
//       InstrumentTab(kNewTab), SeedSearchboxResult("a"),
//       FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"a"),
//       WaitForClassicPopupReady(),
//       InAnyContext(WaitForVerbatimMatch(kPopupWebView, kClassicMatchText,
//       "a")),
//       // Click the match or press enter depending on the test parameter.
//       If([this]() { return ClickMatch(); },
//          Then(InAnyContext(
//              ExecuteJsAt(kPopupWebView, kClassicMatchText, "el =>
//              el.click()")
//                  .SetMustRemainVisible(false))),
//          Else(SendKeyPress(kOmniboxElementId, ui::VKEY_RETURN))),
//       // Ensure google search occurs.
//       WaitForGoogleSearch(kNewTab, {{"q", "a"}}));
// }
