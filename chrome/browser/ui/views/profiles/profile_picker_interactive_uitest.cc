// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_interactive_uitest_base.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using testing::Eq;
using testing::IsFalse;
using testing::IsTrue;

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfilePickerViewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPickerWebContentsId);

const WebContentsInteractionTestUtil::DeepQuery kContinueWithoutAccountButton =
    {"profile-picker-app", "profile-type-choice", "#notNowButton"};
const WebContentsInteractionTestUtil::DeepQuery kAddProfileButton = {
    "profile-picker-app", "profile-picker-main-view", "#addProfile"};

// Waits until the widget bounds change.
class WidgetBoundsChangeWaiter : public views::WidgetObserver {
 public:
  explicit WidgetBoundsChangeWaiter(views::Widget* widget) {
    DCHECK(widget);
    observation_.Observe(widget);
  }

  // Waits until the widget bounds change.
  void Wait() { run_loop_.Run(); }

 private:
  // WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

}  // namespace

struct NavState {
  int entry_count;
  int last_committed_entry_index;
  bool has_pending_entry = false;

  bool operator==(const NavState& other) const {
    return entry_count == other.entry_count &&
           last_committed_entry_index == other.last_committed_entry_index &&
           has_pending_entry == other.has_pending_entry;
  }

  friend std::ostream& operator<<(std::ostream& stream, const NavState& state) {
    return stream << "{entry_count=" << state.entry_count
                  << ", last_committed_entry_index="
                  << state.last_committed_entry_index
                  << ", has_pending_entry=" << state.has_pending_entry << "}";
  }
};

class ProfilePickerInteractiveUiTest
    : public InteractiveBrowserTest,
      public WithProfilePickerInteractiveUiTestHelpers {
 public:
  ProfilePickerInteractiveUiTest() = default;

  ~ProfilePickerInteractiveUiTest() override = default;

  void ShowAndFocusPicker(ProfilePicker::EntryPoint entry_point,
                          const GURL& expected_url = GURL()) {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(entry_point));
    WaitForPickerWidgetCreated();
    view()->SetProperty(views::kElementIdentifierKey, kProfilePickerViewId);
    if (!expected_url.is_empty()) {
      WaitForLoadStop(expected_url);
    }

    EXPECT_TRUE(
        ui_test_utils::ShowAndFocusNativeWindow(widget()->GetNativeWindow()));
  }

  auto GetNavState() {
    return [this]() {
      return NavState{
          .entry_count = web_contents()->GetController().GetEntryCount(),
          .last_committed_entry_index =
              web_contents()->GetController().GetLastCommittedEntryIndex(),
          .has_pending_entry =
              web_contents()->GetController().GetPendingEntry() != nullptr,
      };
    };
  }

  auto HasPendingNav() {
    return [this]() {
      return web_contents()->GetController().GetPendingEntry() != nullptr;
    };
  }

  StateChange Exists(const DeepQuery& where) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExistsEvent);
    StateChange state_change;
    state_change.type = StateChange::Type::kExists;
    state_change.where = where;
    state_change.event = kElementExistsEvent;
    return state_change;
  }

  // Used to wait for screen changes inside of the `profile-picker-app`, in
  // combination with `WaitForStateChange()`.
  //
  // In the profile picker app we modify the displayed URL but don't do a full
  // navigation, so `WaitForWebContentsNavigation()` does not detect these
  // changes.
  StateChange UrlEntryMatches(const GURL& url) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kUrlEntryMatchesEvent);
    StateChange state_change;
    state_change.test_function = base::StringPrintf(
        "(_) => navigation && navigation.currentEntry && "
        "navigation.currentEntry.url === '%s'",
        url.spec().c_str());
    state_change.type = StateChange::Type::kConditionTrue;
    state_change.event = kUrlEntryMatchesEvent;
    return state_change;
  }

  const base::HistogramTester& HistogramTester() const {
    return histogram_tester_;
  }

  auto PressJsButton(const ui::ElementIdentifier web_contents_id,
                     const DeepQuery& button_query) {
    // This can close/navigate the current page, so don't wait for success.
    return ExecuteJsAt(web_contents_id, button_query, "(btn) => btn.click()",
                       ExecuteJsMode::kFireAndForget);
  }

 private:
  base::HistogramTester histogram_tester_;
};

// Checks that the main picker view can be closed with keyboard shortcut.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, CloseWithKeyboard) {
  // Open a new picker.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  SendCloseWindowKeyboardCommand();
  WaitForPickerClosed();
  // Closing the picker does not exit Chrome.
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
}

#if BUILDFLAG(IS_MAC)
// Checks that Chrome be closed with keyboard shortcut. Only MacOS has a
// keyboard shortcut to exit Chrome.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, ExitWithKeyboard) {
  // Open a new picker.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());

  SendQuitAppKeyboardCommand();
  WaitForPickerClosed();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
}
#endif

// Checks that the main picker view can switch to full screen.
IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, FullscreenWithKeyboard) {
  // Open a new picker.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  EXPECT_FALSE(widget()->IsFullscreen());
  WidgetBoundsChangeWaiter bounds_waiter(widget());

  SendToggleFullscreenKeyboardCommand();

  // Fullscreen causes the bounds of the widget to change.
  bounds_waiter.Wait();
  EXPECT_TRUE(widget()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       NavigateBackFromProfileTypeChoiceWithKeyboard) {
  // Simulate walking through the flow starting at the picker so that navigating
  // back to the picker makes sense. Check that the navigation list is populated
  // correctly.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kPickerWebContentsId, web_view()),
      WaitForWebContentsReady(kPickerWebContentsId,
                              GURL("chrome://profile-picker")),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 1,
                                             .last_committed_entry_index = 0})),

      // Advance to the profile type choice screen.
      EnsurePresent(kPickerWebContentsId, kAddProfileButton),
      MoveMouseTo(kPickerWebContentsId, kAddProfileButton), ClickMouse(),
      WaitForStateChange(
          kPickerWebContentsId,
          UrlEntryMatches(GURL("chrome://profile-picker/new-profile"))),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 1})),

      // Navigate back with the keyboard.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_BACK)),
      WithoutDelay(CheckResult(HasPendingNav(), IsTrue())),
      WaitForStateChange(kPickerWebContentsId,
                         UrlEntryMatches(GURL("chrome://profile-picker"))),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 0})),

      // Navigating back once again does nothing.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_BACK)),
      WithoutDelay(CheckResult(HasPendingNav(), IsFalse())));
}

IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest,
                       NavigateBackFromNewProfileWithKeyboard) {
  // Check that when deep-linking into the flow via the "Add profile" menu entry
  // populates the navigation list is populated correctly such that back
  // navigations make sense.
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuAddNewProfile,
                     GURL("chrome://profile-picker/new-profile"));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kPickerWebContentsId, web_view()),
      WaitForWebContentsReady(kPickerWebContentsId,
                              GURL("chrome://profile-picker/new-profile")),

      // Even though we start straight on the "new-profile" page, the picker
      // main view should be loaded under it in the nav stack.
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 1})),

      // Focus the window to ensure the keyboard shortcut reaches it.
      FocusWebContents(kPickerWebContentsId),

      // Navigate back with the keyboard.
      SendAccelerator(kPickerWebContentsId, GetAccelerator(IDC_BACK)),
      WithoutDelay(CheckResult(HasPendingNav(), IsTrue(),
                               /*check_description=*/"HasPendingNav")),
      WaitForStateChange(kPickerWebContentsId,
                         UrlEntryMatches(GURL("chrome://profile-picker"))),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 0})),

      // Navigating back once again does nothing.
      SendAccelerator(kProfilePickerViewId, GetAccelerator(IDC_BACK)),
      WithoutDelay(CheckResult(HasPendingNav(), IsFalse())));
}

IN_PROC_BROWSER_TEST_F(ProfilePickerInteractiveUiTest, ContinueWithoutAccount) {
  ShowAndFocusPicker(ProfilePicker::EntryPoint::kProfileMenuManageProfiles,
                     GURL("chrome://profile-picker"));

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kPickerWebContentsId, web_view()),
      WaitForWebContentsReady(kPickerWebContentsId,
                              GURL("chrome://profile-picker")),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 1,
                                             .last_committed_entry_index = 0})),

      // Advance to the profile type choice screen.
      EnsurePresent(kPickerWebContentsId, kAddProfileButton),
      PressJsButton(kPickerWebContentsId, kAddProfileButton)
          .SetMustRemainVisible(false),
      WaitForStateChange(
          kPickerWebContentsId,
          UrlEntryMatches(GURL("chrome://profile-picker/new-profile"))),
      CheckResult(GetNavState(), Eq(NavState{.entry_count = 2,
                                             .last_committed_entry_index = 1})),

      // Advance to the post signed out flow
      WaitForStateChange(kPickerWebContentsId,
                         Exists(kContinueWithoutAccountButton)),
      PressJsButton(kPickerWebContentsId, kContinueWithoutAccountButton)
          .SetMustRemainVisible(false));

  WaitForPickerClosed();

  HistogramTester().ExpectUniqueSample(
      "Profile.AddNewUser", ProfileMetrics::ADD_NEW_PROFILE_PICKER_LOCAL, 1);
}
