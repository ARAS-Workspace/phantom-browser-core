// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/get_updater_scope.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

class GetUpdaterScopeForCommandLineTest : public testing::Test {
 protected:
  base::CommandLine command_line_ =
      base::CommandLine(GetExecutableRelativePath());
};

TEST_F(GetUpdaterScopeForCommandLineTest, NoParams) {
  EXPECT_EQ(GetUpdaterScopeForCommandLine(command_line_), UpdaterScope::kUser);
  EXPECT_FALSE(IsPrefersForCommandLine(command_line_));
}

TEST_F(GetUpdaterScopeForCommandLineTest, System) {
  command_line_.AppendSwitch(kSystemSwitch);
  EXPECT_EQ(GetUpdaterScopeForCommandLine(command_line_),
            UpdaterScope::kSystem);
  EXPECT_FALSE(IsPrefersForCommandLine(command_line_));
}

}  // namespace updater
