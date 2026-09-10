// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/switch_utils.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SwitchUtilsTest, RemoveSwitches) {
  static const base::CommandLine::CharType* const argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--app=http://www.google.com/"),
      FILE_PATH_LITERAL("--force-first-run"),
      FILE_PATH_LITERAL("--make-default-browser"),
      FILE_PATH_LITERAL("--foo"),
      FILE_PATH_LITERAL("--bar")};
  base::CommandLine cmd_line(std::size(argv), argv);
  EXPECT_FALSE(cmd_line.GetCommandLineString().empty());

  base::CommandLine::SwitchMap switches = cmd_line.GetSwitches();
  EXPECT_EQ(5U, switches.size());

  switches::RemoveSwitchesForAutostart(&switches);
  EXPECT_EQ(2U, switches.size());
  EXPECT_TRUE(cmd_line.HasSwitch("foo"));
  EXPECT_TRUE(cmd_line.HasSwitch("bar"));
}
