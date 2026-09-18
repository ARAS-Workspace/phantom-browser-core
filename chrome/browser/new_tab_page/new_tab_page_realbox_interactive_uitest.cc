// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/check_deref.h"
#include "base/containers/extend.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_interactive_test_mixin.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/shell_dialogs/select_file_dialog.h"

// To debug locally, you can run the test via:
// `out/Default/interactive_ui_tests
// --gtest_filter="*<TEST_NAME>*" --test-launcher-interactive`. The
// `--test-launcher-interactive` flag will pause the test at the very end, after
// the screenshot would've been taken, allowing you to inspect the UI and debug.
//
// To generate an actual screenshot locally, you can run the test with
// `out/Default/interactive_ui_tests
// --gtest_filter="*<TEST_NAME>*" --browser-ui-tests-verify-pixels
// --enable-pixel-output-in-tests --test-launcher-retry-limit=0
// --ui-test-action-timeout=100000
// --skia-gold-local-png-write-directory="/tmp/pixel_test_output"
// --bypass-skia-gold-functionality`. The PNG of the screenshot will be saved to
// the `/tmp/pixel_test_output` directory.

namespace {
using ::testing::ValuesIn;
using DeepQuery = InteractiveBrowserWindowTestApi::DeepQuery;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpElementId);

const DeepQuery kRealbox = {"ntp-app", "ntp-searchbox", "#inputWrapper"};
const DeepQuery kRealboxInput = {"ntp-app", "ntp-searchbox", "#input",
                                 "#input"};
const DeepQuery kRealboxMatch = {"ntp-app", "ntp-searchbox",
                                 "cr-searchbox-dropdown", "cr-searchbox-match",
                                 "#suggestion"};
const DeepQuery kRealboxMatchRemoveButton = {"ntp-app", "ntp-searchbox",
                                             "cr-searchbox-dropdown",
                                             "cr-searchbox-match", "#remove"};
const DeepQuery kVoiceSearchButton = {"ntp-app", "ntp-searchbox",
                                      "#voiceSearchButton"};
const DeepQuery kLensSearchButton = {"ntp-app", "ntp-searchbox",
                                     "#lensSearchButton"};
const DeepQuery kComposeButton = {"ntp-app", "ntp-searchbox", "#composeButton",
                                  "#composeButton"};
const DeepQuery kSearchboxDropdown = {"ntp-app", "ntp-searchbox",
                                      "cr-searchbox-dropdown"};
const DeepQuery kNtpLogo = {"ntp-app", "#logo"};
}  // namespace

class NtpRealboxUiTestBase
    : public SearchboxInteractiveTestMixin<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  NtpRealboxUiTestBase() = default;
  ~NtpRealboxUiTestBase() override = default;

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    SearchboxInteractiveTestMixin<
        WebUiInteractiveTestMixin<InteractiveBrowserTest>>::TearDownOnMainThread();
  }

  MultiStep FocusAndInputText(
      const ui::ElementIdentifier& contents_id,
      const WebContentsInteractionTestUtil::DeepQuery& element) {
    return Steps(ClickElement(contents_id, element),
                 SendKeyPress(contents_id, ui::VKEY_T),
                 SendKeyPress(contents_id, ui::VKEY_E),
                 SendKeyPress(contents_id, ui::VKEY_S),
                 SendKeyPress(contents_id, ui::VKEY_T));
  }

  auto WaitForDialogStateChange(const DeepQuery& where, bool expected_open) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kDialogStateChangeEvent);
    WebContentsInteractionTestUtil::StateChange state_change;
    state_change.event = kDialogStateChangeEvent;
    state_change.where = where;
    state_change.type =
        expected_open
            ? WebContentsInteractionTestUtil::StateChange::Type::kExists
            : WebContentsInteractionTestUtil::StateChange::Type::kDoesNotExist;

    return WaitForStateChange(kNtpElementId, state_change);
  }

  auto WaitForElementVisibilityChange(const DeepQuery& where,
                                      bool expected_visible) {
    return WaitForJsConditionAt(
        kNtpElementId, where,
        expected_visible ? "(el) => el && !el.hasAttribute('hidden')"
                         : "(el) => el && el.hasAttribute('hidden')");
  }

  auto WaitForElementToNotExist(const DeepQuery& where) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementDoesNotExistEvent);
    WebContentsInteractionTestUtil::StateChange state_change;
    state_change.event = kElementDoesNotExistEvent;
    state_change.where = where;
    state_change.type =
        WebContentsInteractionTestUtil::StateChange::Type::kDoesNotExist;

    return WaitForStateChange(kNtpElementId, state_change);
  }

};

class NtpRealboxDefaultExperienceInteractiveTest : public NtpRealboxUiTestBase {
 public:
  NtpRealboxDefaultExperienceInteractiveTest() {
    content::SpeechRecognitionManager::SetManagerForTesting(
        &fake_speech_recognition_manager_);
    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::kOmniboxAppendInvocationSource, {}}},
        {ntp_features::kNtpNextFeatures,
         omnibox::kVoiceSearchCoherenceSearchbox});
  }

 protected:
  content::FakeSpeechRecognitionManager fake_speech_recognition_manager_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       DefaultExperienceRealboxUI) {
  RunTestSequence(
      // Load NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Wait for Realbox to render.
      WaitForElementToRender(kNtpElementId, kRealbox),
      // Wait for Voice Search, Lens, and AI Mode buttons to render.
      WaitForElementToRender(kNtpElementId, kVoiceSearchButton),
      WaitForElementToRender(kNtpElementId, kLensSearchButton),
      WaitForElementToRender(kNtpElementId, kComposeButton),
      // Verify the placeholder text is steady.
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el && el.getAnimations().length === 0"),
      // Click into the Searchbox.
      ClickElement(kNtpElementId, kRealboxInput),
      // Verify that the placeholder text disappears.
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el && window.getComputedStyle(el, "
                           "'::placeholder').visibility === 'hidden'"),
      // Type text into Realbox and click AIM Button.
      SendKeyPress(kNtpElementId, ui::VKEY_T),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el && el.value === 't'"),
      ClickElement(kNtpElementId, kComposeButton),
      // Wait for the page to navigate to Google SRP.
      WaitForGoogleSearch(kNtpElementId, {{"q", "t"}, {"udm", "50"}}));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       ClickOutsideAndEscapeBehavior) {
  RunTestSequence(
      // Load NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Wait for Realbox to render.
      WaitForElementToRender(kNtpElementId, kRealboxInput),
      // Wait for Voice Search, Lens, and AI Mode buttons to render.
      WaitForElementToRender(kNtpElementId, kVoiceSearchButton),
      WaitForElementToRender(kNtpElementId, kLensSearchButton),
      WaitForElementToRender(kNtpElementId, kComposeButton),
      // Seed history results to ensure the dropdown is populated.
      SeedSearchboxResult("a"),
      // Click on Realbox.
      ClickElement(kNtpElementId, kRealboxInput),
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/true),
      // Verify: Only AIM Button is Visible in searchbox
      WaitForElementVisibilityChange(kComposeButton, /*expected_visible=*/true),
      WaitForElementToNotExist(kVoiceSearchButton),
      WaitForElementToNotExist(kLensSearchButton),
      // Type Something into Realbox.
      SendKeyPress(kNtpElementId, ui::VKEY_A),
      // Wait for the verbatim match to render.
      WaitForVerbatimMatch(kNtpElementId, kRealboxMatch, "a"),
      // Verify: Clicking outside the realbox should close suggestions dropdown,
      // but leave the text inside the realbox visible.
      MoveMouseTo(kNtpElementId, kNtpLogo), ClickMouse(),
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/false),
      CheckJsResultAt(kNtpElementId, kRealboxInput,
                      "(el) => el && el.value === 'a'"),
      // Check focus is lost
      CheckJsResultAt(kNtpElementId, kRealboxInput,
                      "(el) => !el.matches(':focus')"),
      // Click on realbox and press ‘ESC’ button
      ClickElement(kNtpElementId, kRealboxInput),
      SendKeyPress(kNtpElementId, ui::VKEY_ESCAPE),
      // Verify: Text inside realbox is cleared, but the focus remains.
      // Also verify that the dropdown is closed.
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/false),
      CheckJsResultAt(kNtpElementId, kRealboxInput,
                      "(el) => el && el.value === ''"),
      CheckJsResultAt(kNtpElementId, kRealboxInput,
                      "(el) => el.matches(':focus')"),
      // Verify: Realbox contains AIM Button, Voice Search button, Lens button
      WaitForElementVisibilityChange(kComposeButton, /*expected_visible=*/true),
      WaitForElementToRender(kNtpElementId, kVoiceSearchButton),
      WaitForElementToRender(kNtpElementId, kLensSearchButton));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       VoiceSearchNavigatesToGoogleSearch) {
  const std::string query = "testing";
  const DeepQuery kVoiceSearchOverlayDialog = {
      "ntp-app", "ntp-voice-search-overlay", "#dialog"};

  // Configure the mock to pause before sending a response so we can verify
  // the overlay UI.
  fake_speech_recognition_manager_.set_should_send_fake_response(false);
  fake_speech_recognition_manager_.SetFakeResult(query, /*is_final=*/true);

  RunTestSequence(
      // Load NTP.
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Wait for Realbox to render.
      WaitForElementToRender(kNtpElementId, kRealbox),
      // Wait for Voice Search button to render.
      WaitForElementToRender(kNtpElementId, kVoiceSearchButton),
      // Click on Voice Search button.
      ClickElement(kNtpElementId, kVoiceSearchButton),
      // Verify that the voice search overlay dialog appears and is open.
      WaitForElementToRender(kNtpElementId, kVoiceSearchOverlayDialog),
      WaitForDialogStateChange(kVoiceSearchOverlayDialog,
                               /*expected_open=*/true),
      // Send the mock response.
      Do([&]() {
        fake_speech_recognition_manager_.SendFakeResponse(
            /*end_recognition=*/true,
            /*on_fake_response_sent=*/base::DoNothing());
      }),
      // Wait for the page to navigate to Google SRP.
      WaitForGoogleSearch(kNtpElementId, {{"q", query}}));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       KeyboardNavigationAndIndexCycling) {
  RunTestSequence(
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      WaitForElementToRender(kNtpElementId, kRealboxInput),
      // Seed history results to ensure the dropdown is populated.
      SeedSearchboxResult("h"),
      // Click realbox input to focus it and trigger the dropdown/scrim.
      ClickElement(kNtpElementId, kRealboxInput),
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/true),
      SendKeyPress(kNtpElementId, ui::VKEY_H),
      // Wait for the verbatim match to render.
      WaitForVerbatimMatch(kNtpElementId, kRealboxMatch, "h"),
      // Press DOWN to select the first suggestion.
      SendKeyPress(kNtpElementId, ui::VKEY_DOWN),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'suggestion-1'"),
      // Press DOWN to select the next suggestion.
      SendKeyPress(kNtpElementId, ui::VKEY_DOWN),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'suggestion-2'"),
      // Press DOWN to wrap around to the first item (which is the verbatim "").
      SendKeyPress(kNtpElementId, ui::VKEY_DOWN),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'h'"),
      // Press UP to wrap around to the bottom item.
      SendKeyPress(kNtpElementId, ui::VKEY_UP),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'suggestion-2'"),
      // Press ENTER to navigate.
      SendKeyPress(kNtpElementId, ui::VKEY_RETURN),
      // Ensure google search occurs.
      WaitForGoogleSearch(kNtpElementId,
                          {{"q", "suggestion-2"}, {"source", "chrome.rb"}}));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       RemoveSuggestionViaClick) {
  RunTestSequence(
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      WaitForElementToRender(kNtpElementId, kRealboxInput),
      // Seed history results to populate the dropdown
      SeedSearchboxResult("aimode"),
      // Click on Realbox to show the dropdown
      ClickElement(kNtpElementId, kRealboxInput),
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/true),
      SendKeyPress(kNtpElementId, ui::VKEY_A),
      // Wait for the inline autocompleted verbatim match to render.
      WaitForVerbatimMatch(kNtpElementId, kRealboxMatch, "aimode"),
      // Wait for the remove button to render and become visible
      WaitForElementToRender(kNtpElementId, kRealboxMatchRemoveButton),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'aimode'"),
      // Click the remove button
      ClickElement(kNtpElementId, kRealboxMatchRemoveButton),
      // After removing the inline autocomplete match, the input should revert
      // to "a"
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'a'"));
}

IN_PROC_BROWSER_TEST_F(NtpRealboxDefaultExperienceInteractiveTest,
                       RemoveSuggestionViaKeyboard) {
  RunTestSequence(
      AddInstrumentedTab(kNtpElementId, chrome::ChromeUINewTabURLAsGURL()),
      WaitForElementToRender(kNtpElementId, kRealboxInput),
      // Seed history result to populate the dropdown
      SeedSearchboxResult("a"), SeedSearchboxResult("b"),
      // Click on Realbox to show the dropdown
      ClickElement(kNtpElementId, kRealboxInput),
      WaitForElementVisibilityChange(kSearchboxDropdown,
                                     /*expected_visible=*/true),
      // Pressing Tab should focus the AIM button.
      SendKeyPress(kNtpElementId, ui::VKEY_TAB),
      WaitForJsConditionAt(kNtpElementId, kComposeButton,
                           "(el) => el && el.matches(':focus')"),
      // Pressing Tab again should focus the inline autocomplete match.
      SendKeyPress(kNtpElementId, ui::VKEY_TAB),
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'b'"),
      // Pressing Tab again should focus the remove button.
      SendKeyPress(kNtpElementId, ui::VKEY_TAB),
      WaitForJsConditionAt(kNtpElementId, kRealboxMatchRemoveButton,
                           "(el) => el && el.matches(':focus')"),
      // Trigger the remove button via ENTER
      SendKeyPress(kNtpElementId, ui::VKEY_RETURN),
      // After removing the current match, the next match remove button should
      // be focused.
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === 'a'"),
      SendKeyPress(kNtpElementId, ui::VKEY_RETURN),
      // After all matches are removed, the input should be empty.
      WaitForJsConditionAt(kNtpElementId, kRealboxInput,
                           "(el) => el.value === ''"));
}
