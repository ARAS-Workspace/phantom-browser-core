// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/accelerator_table.h"

#include <stddef.h>

#include <algorithm>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"

namespace chrome {

namespace {

struct Cmp {
  bool operator()(const AcceleratorMapping& lhs,
                  const AcceleratorMapping& rhs) const {
    if (lhs.keycode != rhs.keycode) {
      return lhs.keycode < rhs.keycode;
    }
    return lhs.modifiers < rhs.modifiers;
    // Do not check |command_id|.
  }
};

}  // namespace

TEST(AcceleratorTableTest, CheckDuplicatedAccelerators) {
  base::flat_set<AcceleratorMapping, Cmp> accelerators;
  for (const auto& entry : GetAcceleratorList()) {
    EXPECT_TRUE(accelerators.insert(entry).second)
        << "Duplicated accelerator: " << entry.keycode << ", "
        << (entry.modifiers & ui::EF_SHIFT_DOWN) << ", "
        << (entry.modifiers & ui::EF_CONTROL_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALT_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALTGR_DOWN);
  }
}

TEST(AcceleratorTableTest, PrintKeySupport) {
  int command_id = -1;
  for (const auto& entry : GetAcceleratorList()) {
    if (entry.keycode == ui::VKEY_PRINT) {
      command_id = entry.command_id;
    }
  }
  EXPECT_EQ(-1, command_id);
}

TEST(AcceleratorTableTest, OpenFeedbackWithSearchBasedAccelerator) {
  int command_id = -1;
  for (const auto& entry : GetAcceleratorList()) {
    if (entry.keycode == ui::VKEY_I &&
        entry.modifiers == (ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN)) {
      command_id = entry.command_id;
    }
  }

  EXPECT_EQ(-1, command_id);
}

// A test fixture for testing GetAcceleratorList().
class GetAcceleratorListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Make sure that previous tests don't affect this test.
    ClearAcceleratorListForTesting();
  }

  void TearDown() override {
    // Make sure that this test doesn't affect following tests.
    ClearAcceleratorListForTesting();
  }
};

// Verify that the shortcuts for DevTools are enabled.
TEST_F(GetAcceleratorListTest, DevToolsAreEnabled) {
  // Verify there is a mapping that is associated to IDC_DEV_TOOLS_TOGGLE.
  std::vector<AcceleratorMapping> list = GetAcceleratorList();
  auto iter = std::find_if(list.begin(), list.end(), [](auto mapping) {
    return mapping.command_id == IDC_DEV_TOOLS_TOGGLE;
  });
  EXPECT_NE(iter, list.end());
}

}  // namespace chrome
