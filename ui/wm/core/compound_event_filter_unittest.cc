// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/compound_event_filter.h"

#include "build/build_config.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/public/activation_client.h"

namespace {

}

namespace wm {

namespace {

// An event filter that consumes all gesture events.
class ConsumeGestureEventFilter : public ui::EventHandler {
 public:
  ConsumeGestureEventFilter() {}

  ConsumeGestureEventFilter(const ConsumeGestureEventFilter&) = delete;
  ConsumeGestureEventFilter& operator=(const ConsumeGestureEventFilter&) =
      delete;

  ~ConsumeGestureEventFilter() override {}

 private:
  // Overridden from ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* e) override { e->StopPropagation(); }
};

}  // namespace

typedef aura::test::AuraTestBase CompoundEventFilterTest;

// Tests that if an event filter consumes a gesture, then it doesn't focus the
// window.
TEST_F(CompoundEventFilterTest, FilterConsumedGesture) {
  std::unique_ptr<CompoundEventFilter> compound_filter(new CompoundEventFilter);
  std::unique_ptr<ui::EventHandler> gesure_handler(
      new ConsumeGestureEventFilter);
  compound_filter->AddHandler(gesure_handler.get());
  aura::Env::GetInstance()->AddPreTargetHandler(compound_filter.get());
  aura::test::TestWindowDelegate delegate;
  DCHECK(root_window());
  std::unique_ptr<aura::Window> window =
      aura::test::CreateTestWindow({.delegate = &delegate,
                                    .parent = root_window(),
                                    .bounds = {5, 5, 100, 100},
                                    .window_id = 1234});
  window->Show();

  EXPECT_TRUE(window->CanFocus());
  EXPECT_FALSE(window->HasFocus());

  // Tap on the window should not focus it since the filter will be consuming
  // the gestures.
  ui::test::EventGenerator generator(root_window(), gfx::Point(50, 50));
  generator.PressTouch();
  EXPECT_FALSE(window->HasFocus());

  compound_filter->RemoveHandler(gesure_handler.get());
  aura::Env::GetInstance()->RemovePreTargetHandler(compound_filter.get());
}

// Verifies we don't attempt to hide the mouse when the mouse is down and a
// touch event comes in.
TEST_F(CompoundEventFilterTest, DontHideWhenMouseDown) {
  ui::test::EventGenerator event_generator(root_window());

  std::unique_ptr<CompoundEventFilter> compound_filter(new CompoundEventFilter);
  aura::Env::GetInstance()->AddPreTargetHandler(compound_filter.get());
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window =
      aura::test::CreateTestWindow({.delegate = &delegate,
                                    .parent = root_window(),
                                    .bounds = {5, 5, 100, 100},
                                    .window_id = 1234});
  window->Show();

  aura::test::TestCursorClient cursor_client(root_window());

  // Move and press the mouse over the window.
  event_generator.MoveMouseTo(10, 10);
  EXPECT_TRUE(cursor_client.IsMouseEventsEnabled());
  event_generator.PressLeftButton();
  EXPECT_TRUE(cursor_client.IsMouseEventsEnabled());
  EXPECT_TRUE(aura::Env::GetInstance()->IsMouseButtonDown());

  // Do a touch event. As the mouse button is down this should not disable mouse
  // events.
  event_generator.PressTouch();
  EXPECT_TRUE(cursor_client.IsMouseEventsEnabled());
  aura::Env::GetInstance()->RemovePreTargetHandler(compound_filter.get());
}

}  // namespace wm
