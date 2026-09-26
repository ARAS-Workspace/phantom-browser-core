// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/pointer_device.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(PointerDeviceTest, ScopedOverrideBasic) {
  {
    ScopedSetPointerAndHoverTypesForTesting scoper(POINTER_TYPE_FINE,
                                                   HOVER_TYPE_HOVER);
    auto [pointer_types, hover_types] = GetAvailablePointerAndHoverTypes();
    EXPECT_EQ(pointer_types, POINTER_TYPE_FINE);
    EXPECT_EQ(hover_types, HOVER_TYPE_HOVER);
  }
}

TEST(PointerDeviceTest, PrimaryHoverTypeWithFinePointerOnly) {
  ScopedSetPointerAndHoverTypesForTesting scoper(POINTER_TYPE_FINE,
                                                 HOVER_TYPE_HOVER);
  EXPECT_EQ(GetPrimaryHoverType(), HOVER_TYPE_HOVER);
}

TEST(PointerDeviceTest, PrimaryHoverTypeWithNoPointer) {
  ScopedSetPointerAndHoverTypesForTesting scoper(POINTER_TYPE_NONE,
                                                 HOVER_TYPE_NONE);
  EXPECT_EQ(GetPrimaryHoverType(), HOVER_TYPE_NONE);
}

TEST(PointerDeviceTest, PrimaryPointerType) {
  {
    ScopedSetPointerAndHoverTypesForTesting scoper(POINTER_TYPE_COARSE,
                                                   HOVER_TYPE_NONE);
    EXPECT_EQ(GetPrimaryPointerType(), POINTER_TYPE_COARSE);
  }
  {
    ScopedSetPointerAndHoverTypesForTesting scoper(POINTER_TYPE_FINE,
                                                   HOVER_TYPE_HOVER);
    EXPECT_EQ(GetPrimaryPointerType(), POINTER_TYPE_FINE);
  }
}

}  // namespace ui
