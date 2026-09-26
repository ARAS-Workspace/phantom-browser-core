// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/mouse_cursor_overlay_controller_unittest.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#endif

namespace content {

using testing::_;
using testing::Mock;

MockOverlay::MockOverlay() = default;
MockOverlay::~MockOverlay() = default;

MouseCursorOverlayControllerTestBase::MouseCursorOverlayControllerTestBase()
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  scoped_feature_list_.InitWithFeatures({blink::features::kCapturedMouseEvents},
                                        {});
}

MouseCursorOverlayControllerTestBase::~MouseCursorOverlayControllerTestBase() =
    default;

void MouseCursorOverlayControllerTestBase::RunRestrictsToWebContentsTest() {
  MouseCursorOverlayController controller;

  auto target_web_contents =
      TestWebContents::Create(browser_context(), nullptr);

  const gfx::Rect target_bounds(10, 40, 50, 50);
  SetupCaptureTarget(target_web_contents.get(), target_bounds);

  controller.SetTargetView(GetTargetView(), target_web_contents.get());

  auto overlay_ptr = std::make_unique<MockOverlay>();
  MockOverlay* overlay = overlay_ptr.get();

  controller.Start(std::move(overlay_ptr),
                   base::SingleThreadTaskRunner::GetCurrentDefault());

  InitializeEventGenerator();

  // Inside bounds.
  gfx::Point mouse_pos(20, 45);
  gfx::Point expected_pos =
      GetExpectedCapturedPosition(mouse_pos, target_bounds);
  EXPECT_CALL(*overlay, OnCapturedMouseEvent(expected_pos)).Times(1);
  SendMouseMove(mouse_pos);
  task_environment()->FastForwardBy(base::Seconds(1));
  Mock::VerifyAndClearExpectations(overlay);

  // Outside bounds.
  gfx::Point mouse_pos_outside(60, 60);
  EXPECT_CALL(*overlay, OnCapturedMouseEvent(
                            MouseCursorOverlayController::kOutsideSurface))
      .Times(1);
  SendMouseMove(mouse_pos_outside);
  task_environment()->FastForwardBy(base::Seconds(1));
  Mock::VerifyAndClearExpectations(overlay);

  controller.SetTargetView(gfx::NativeView(), nullptr);
  controller.Stop();
}

#if defined(USE_AURA)

class MouseCursorOverlayControllerAuraTest
    : public MouseCursorOverlayControllerTestBase {
 protected:
  void SetupCaptureTarget(WebContents* target_web_contents,
                          const gfx::Rect& bounds) override {
    target_web_contents->GetNativeView()->SetBounds(bounds);

    // Add child
    web_contents()->GetNativeView()->AddChild(
        target_web_contents->GetNativeView());
    web_contents()->GetNativeView()->SetBounds(gfx::Rect(0, 0, 100, 100));
    target_web_contents->GetNativeView()->Show();

    // Add the main view to the root window so it can receive events
    root_window()->AddChild(web_contents()->GetNativeView());
    web_contents()->GetNativeView()->Show();
  }

  gfx::NativeView GetTargetView() override { return root_window(); }

  void InitializeEventGenerator() override {
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window());
  }

  void SendMouseMove(const gfx::Point& position_in_parent) override {
    generator_->MoveMouseTo(
        web_contents()->GetNativeView()->GetBoundsInScreen().origin() +
        position_in_parent.OffsetFromOrigin());
  }

  gfx::Point GetExpectedCapturedPosition(
      const gfx::Point& position_in_parent,
      const gfx::Rect& target_bounds) override {
    return position_in_parent - target_bounds.OffsetFromOrigin();
  }

 private:
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

TEST_F(MouseCursorOverlayControllerAuraTest, RestrictsToWebContents) {
  RunRestrictsToWebContentsTest();
}

#endif

}  // namespace content
