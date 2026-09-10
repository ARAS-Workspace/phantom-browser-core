// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include <stddef.h>

#include <optional>

#include "build/build_config.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/wm/core/cursor_loader.h"

namespace content {
namespace {

class TestScreen : public display::test::TestScreen {
 public:
  explicit TestScreen(display::Display display)
      : display::test::TestScreen(/*create_display=*/false,
                                  /*register_screen=*/true) {
    display_list().AddDisplay(display, display::DisplayList::Type::PRIMARY);
  }
};

TEST(WebCursorTest, DefaultConstructor) {
  WebCursor webcursor;
  EXPECT_EQ(ui::mojom::CursorType::kNull, webcursor.cursor().type());
}

TEST(WebCursorTest, WebCursorCursorConstructor) {
  ui::Cursor cursor(ui::mojom::CursorType::kHand);
  WebCursor webcursor(cursor);
  EXPECT_EQ(cursor, webcursor.cursor());
}

TEST(WebCursorTest, WebCursorCursorConstructorCustom) {
  const ui::Cursor cursor = ui::Cursor::NewCustom(
      gfx::test::CreateBitmap(/*size=*/32), gfx::Point(10, 20), 2.0f);
  WebCursor webcursor(cursor);
  EXPECT_EQ(cursor, webcursor.cursor());

#if defined(USE_AURA)
  auto cursor_shape_client = std::make_unique<wm::CursorLoader>();
  aura::client::SetCursorShapeClient(cursor_shape_client.get());

  // Test if the custom cursor is correctly cached and updated
  // on aura platform.
  EXPECT_FALSE(webcursor.has_custom_cursor_for_test());
  gfx::NativeCursor native_cursor = webcursor.GetNativeCursor();
  EXPECT_EQ(gfx::Point(5, 10), native_cursor.custom_hotspot());
  EXPECT_TRUE(webcursor.has_custom_cursor_for_test());

  // Test if the rotating custom cursor works correctly.
  display::Display display(/*id=*/1);
  display.set_panel_rotation(display::Display::ROTATE_90);
  TestScreen screen(display);
  webcursor.UpdateDisplayInfoForWindow(nullptr);

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(webcursor.has_custom_cursor_for_test());
#else
  EXPECT_TRUE(webcursor.has_custom_cursor_for_test());
#endif

  native_cursor = webcursor.GetNativeCursor();
  EXPECT_TRUE(webcursor.has_custom_cursor_for_test());

#if BUILDFLAG(IS_CHROMEOS)
  // Hotspot should be scaled & rotated. We're using the icon created for 2.0,
  // on the display with dsf=1.0, so the hotspot should be
  // ((32 - 20) / 2, 10 / 2) = (6, 5).
  EXPECT_EQ(gfx::Point(6, 5), native_cursor.custom_hotspot());
#else
  // For non-CrOS platforms, the cursor mustn't be rotated as logical and
  // physical location is the same.
  EXPECT_EQ(gfx::Point(5, 10), native_cursor.custom_hotspot());
#endif

  aura::client::SetCursorShapeClient(nullptr);
#endif  // defined(USE_AURA)
}

#if defined(USE_AURA)
TEST(WebCursorTest, CursorScaleFactor) {
  auto cursor_shape_client = std::make_unique<wm::CursorLoader>();
  aura::client::SetCursorShapeClient(cursor_shape_client.get());

  constexpr float kImageScale = 2.0f;
  constexpr float kDeviceScale = 4.2f;

  WebCursor webcursor(ui::Cursor::NewCustom(
      gfx::test::CreateBitmap(/*size=*/128), gfx::Point(0, 1), kImageScale));

  display::Display display(/*id=*/1);
  display.set_device_scale_factor(kDeviceScale);
  TestScreen screen(display);
  webcursor.UpdateDisplayInfoForWindow(nullptr);

#if BUILDFLAG(IS_CHROMEOS)
  // In Ash, the size of the cursor is capped at 64px unless the hardware
  // advertises support for bigger cursors.
  const gfx::Size kDefaultMaxSize = gfx::Size(64, 64);
  EXPECT_EQ(gfx::SkISizeToSize(
                webcursor.GetNativeCursor().custom_bitmap().dimensions()),
            kDefaultMaxSize);
#else
  EXPECT_EQ(
      gfx::SkISizeToSize(
          webcursor.GetNativeCursor().custom_bitmap().dimensions()),
      gfx::ScaleToFlooredSize(gfx::Size(128, 128), kDeviceScale / kImageScale));
#endif

  // The scale factor of the cursor image should match the device scale factor,
  // regardless of the cursor size.
  EXPECT_EQ(webcursor.GetNativeCursor().image_scale_factor(), kDeviceScale);

  aura::client::SetCursorShapeClient(nullptr);
}

#endif  // defined(USE_AURA)

}  // namespace
}  // namespace content
