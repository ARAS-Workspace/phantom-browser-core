// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_impl.h"
#include "components/user_education/common/help_bubble/help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace {
constexpr char16_t kBubbleBodyText[] = u"Bubble body text.";
constexpr char16_t kBubbleButtonText[] = u"Button";
constexpr char16_t kCloseButtonAltText[] = u"Close";
}  // namespace

class HelpBubbleFactoryWebUIInteractiveUiTest : public InteractiveBrowserTest {
 public:
  HelpBubbleFactoryWebUIInteractiveUiTest() = default;
  ~HelpBubbleFactoryWebUIInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }

  auto ShowHelpBubble(ElementSpecifier element) {
    return InAnyContext(
        AfterShow(
            element,
            base::BindLambdaForTesting(
                [this](ui::InteractionSequence* seq, ui::TrackedElement* el) {
                  help_bubble_ = GetHelpBubbleFactory()->CreateHelpBubble(
                      el, GetHelpBubbleParams());
                  if (!help_bubble_) {
                    LOG(ERROR) << "Failed to create help bubble.";
                    seq->FailForTesting();
                  }
                }))
            .SetDescription("ShowHelpBubble"));
  }

  auto CloseHelpBubble() {
    return Do(base::BindLambdaForTesting([this]() {
      help_bubble_->Close(
          user_education::HelpBubble::CloseReason::kProgrammaticallyClosed);
    }));
  }

  auto CheckHandlerHasHelpBubble(ElementSpecifier anchor,
                                 bool has_help_bubble) {
    return InAnyContext(
        CheckElement(
            anchor,
            [](ui::TrackedElement* el) {
              return el->AsA<ui::TrackedElementWebUI>()
                  ->handler()
                  ->GetHelpBubbleHandler()
                  ->IsHelpBubbleShowingForTesting(el->identifier());
            },
            has_help_bubble)
            .SetDescription(
                base::StringPrintf("CheckHandlerHasHelpBubble(%s)",
                                   base::ToString(has_help_bubble))));
  }

  auto Cleanup() {
    return Do(base::BindLambdaForTesting([this]() { help_bubble_.reset(); }));
  }

 protected:
  std::unique_ptr<user_education::HelpBubble> help_bubble_;

 private:
  static user_education::HelpBubbleParams GetHelpBubbleParams() {
    user_education::HelpBubbleButtonParams button_params;
    button_params.is_default = true;
    button_params.text = kBubbleButtonText;
    button_params.callback = base::DoNothing();
    user_education::HelpBubbleParams params;
    params.body_text = kBubbleBodyText;
    params.close_button_alt_text = kCloseButtonAltText;
    params.buttons.emplace_back(std::move(button_params));
    return params;
  }

  user_education::HelpBubbleFactoryRegistry* GetHelpBubbleFactory() {
    auto* const controller = UserEducationServiceFactory::GetForBrowserContext(
                                 browser()->GetProfile())
                                 ->GetFeaturePromoControllerForTesting();
    return static_cast<user_education::FeaturePromoControllerImpl*>(controller)
        ->bubble_factory_registry();
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryWebUIInteractiveUiTest,
                       ShowEmbeddedHelpBubbleAndCloseViaExternalApi) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserTabId);
  static const DeepQuery kPathToHelpBubbleBody = {"user-education-internals",
                                                  "#IPH_WebUiHelpBubbleTest",
                                                  "help-bubble", "#topBody"};
  RunTestSequence(
      InstrumentTab(kBrowserTabId),
      NavigateWebContents(kBrowserTabId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      ShowHelpBubble(kWebUIIPHDemoElementIdentifier),

      // Verify that the handler believes that the anchor has a help bubble.
      CheckHandlerHasHelpBubble(kWebUIIPHDemoElementIdentifier, true),

      CheckJsResultAt(kBrowserTabId, kPathToHelpBubbleBody,
                      "el => el.innerText", base::UTF16ToUTF8(kBubbleBodyText)),
      CloseHelpBubble(),

      // Verify that the handler no longer believes that the anchor has a help
      // bubble.
      CheckHandlerHasHelpBubble(kWebUIIPHDemoElementIdentifier, false),

      Cleanup());
}

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryWebUIInteractiveUiTest,
                       ShowEmbeddedHelpBubbleAndCloseViaClick) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kHelpBubbleHiddenEvent);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserTabId);
  static const DeepQuery kPathToHelpBubbleCloseButton = {
      "user-education-internals", "#IPH_WebUiHelpBubbleTest", "help-bubble",
      "#close"};
  StateChange bubble_hidden;
  bubble_hidden.type = StateChange::Type::kDoesNotExist;
  bubble_hidden.where = kPathToHelpBubbleCloseButton;
  bubble_hidden.event = kHelpBubbleHiddenEvent;

  RunTestSequence(
      InstrumentTab(kBrowserTabId),
      NavigateWebContents(kBrowserTabId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      ShowHelpBubble(kWebUIIPHDemoElementIdentifier),

      ExecuteJsAt(kBrowserTabId, kPathToHelpBubbleCloseButton,
                  "el => el.click()"),
      WaitForStateChange(kBrowserTabId, bubble_hidden),

      // Verify that the handler no longer believes that the anchor has a help
      // bubble.
      CheckHandlerHasHelpBubble(kWebUIIPHDemoElementIdentifier, false),

      Cleanup());
}
