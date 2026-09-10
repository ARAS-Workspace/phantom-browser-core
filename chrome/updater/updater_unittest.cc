// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/updater/util/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace updater {

// Tests the updater process returns 0 when run with --test argument.
TEST(UpdaterTest, UpdaterExitCode) {
  base::FilePath out_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &out_dir));
  const base::FilePath updater =
      out_dir.Append(updater::GetExecutableRelativePath());

  base::LaunchOptions options;

  base::CommandLine command_line(updater);
  command_line.AppendSwitch("test");
  auto process = base::LaunchProcess(command_line, options);
  ASSERT_TRUE(process.IsValid());
  int exit_code = -1;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
  EXPECT_EQ(0, exit_code);
}

}  // namespace updater
