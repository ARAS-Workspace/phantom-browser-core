// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_impl.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/common/tutorial/tutorial.h"
#include "components/user_education/common/tutorial/tutorial_description.h"
#include "components/user_education/common/tutorial/tutorial_registry.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/views/help_bubble_views.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace {
constexpr char kTestTutorialId[] = "TutorialInteractiveUitest Tutorial";
constexpr char kTestTutorialMetricPrefix[] = "Test";
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEventType1);
}  // namespace

using user_education::FeaturePromoControllerImpl;
using user_education::HelpBubbleArrow;
using user_education::HelpBubbleViews;
using user_education::TutorialDescription;
using user_education::TutorialService;

class TutorialInteractiveUitest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    GetTutorialService()->tutorial_registry()->AddTutorial(
        kTestTutorialId, GetDefaultTutorialDescription());
  }

  void TearDownOnMainThread() override {
    auto* const service = GetTutorialService();
    service->CancelTutorialIfRunning();
    service->tutorial_registry()->RemoveTutorialForTesting(kTestTutorialId);
  }

  static void ClearEventQueue() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  TutorialService* GetTutorialService() {
    return static_cast<FeaturePromoControllerImpl*>(
               UserEducationServiceFactory::GetForBrowserContext(
                   browser()->GetProfile())
                   ->GetFeaturePromoControllerForTesting())
        ->tutorial_service_for_testing();
  }

  ui::TrackedElement* GetElement(ui::ElementIdentifier id) {
    return BrowserElements::From(browser())->GetElement(id);
  }

  TutorialDescription GetDefaultTutorialDescription() {
    return TutorialDescription::Create<kTestTutorialMetricPrefix>(
        TutorialDescription::BubbleStep(kToolbarAppMenuButtonElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
            .SetBubbleArrow(HelpBubbleArrow::kTopRight),
        TutorialDescription::EventStep(kCustomEventType1),
        TutorialDescription::HiddenStep::WaitForActivated(
            kToolbarAppMenuButtonElementId),
        TutorialDescription::BubbleStep(kToolbarAppMenuButtonElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
            .SetBubbleArrow(HelpBubbleArrow::kTopRight));
  }

  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(TutorialInteractiveUitest, SampleTutorial) {
  UNCALLED_MOCK_CALLBACK(TutorialService::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(TutorialService::AbortedCallback, aborted);

  GetTutorialService()->StartTutorial(
      kTestTutorialId, BrowserElements::From(browser())->GetContext(),
      completed.Get(), aborted.Get());
  ClearEventQueue();
  EXPECT_TRUE(GetTutorialService()->IsRunningTutorial());

  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      GetElement(kTabStripElementId), kCustomEventType1);
  ClearEventQueue();

  ui::test::InteractionTestUtil test_util;
  test_util.AddSimulator(
      std::make_unique<views::test::InteractionTestUtilSimulatorViews>());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util.PressButton(GetElement(kToolbarAppMenuButtonElementId)));
  ClearEventQueue();

  // Simulate click on close button.
  EXPECT_ASYNC_CALL_IN_SCOPE(
      completed, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          AsViewClass<user_education::HelpBubbleView>(
              GetTutorialService()
                  ->currently_displayed_bubble_for_testing()
                  ->AsA<HelpBubbleViews>()
                  ->bubble_view_for_testing())
              ->GetDefaultButtonForTesting(),
          ui::test::InteractionTestUtil::InputType::kKeyboard));

  const auto histogram_name =
      base::StringPrintf("Tutorial.%s.Completion", kTestTutorialMetricPrefix);
  histogram_tester_.ExpectBucketCount(histogram_name, 0, 0);
  histogram_tester_.ExpectBucketCount(histogram_name, 1, 1);
}
