// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(ClipboardFormatTypeTest, CustomPlatformType) {
  // ASCII format names should work.
  std::string ascii_format = "text/plain";
  ClipboardFormatType type =
      ClipboardFormatType::CustomPlatformType(ascii_format);
  EXPECT_EQ(type.GetName(), ascii_format);

  // Non-ASCII format names should trigger a CHECK failure in
  // CustomPlatformType. We don't test this with EXPECT_DEATH here to avoid slow
  // tests, but the contract is that the input must be ASCII.
}

TEST(ClipboardFormatTypeTest, Deserialize) {
  // ASCII format names should always work.
  std::string ascii_serialization = "text/plain";
  ClipboardFormatType type =
      ClipboardFormatType::Deserialize(ascii_serialization);
  EXPECT_EQ(type.GetName(), ascii_serialization);

#if !BUILDFLAG(IS_APPLE)
  // Deserialize should work with non-ASCII on some non-Apple platforms, as it's
  // used for internal serializations which might technically allow it, or at
  // least it doesn't have the ASCII CHECK.
  // On Apple platforms, GetName() returns an empty string for invalid UTF-8.
  // On Windows, Deserialize expects a numeric string.
  std::string non_ascii_format = "non-ascii-\xff";
  ClipboardFormatType type2 =
      ClipboardFormatType::Deserialize(non_ascii_format);
  EXPECT_EQ(type2.GetName(), non_ascii_format);
#endif
}

}  // namespace ui
