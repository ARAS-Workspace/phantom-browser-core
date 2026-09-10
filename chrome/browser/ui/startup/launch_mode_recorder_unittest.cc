// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/launch_mode_recorder.h"

#include <optional>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kLaunchModeMetric[] = "Launch.Mode2";

struct PathKeyAndLaunchMode {
  int path_key;
  LaunchMode launch_mode;
};

}  // namespace

class LaunchModeRecorderTest : public testing::Test {
 protected:
  void ComputeLaunchModeAndVerify(const base::CommandLine& cmd_line,
                                  LaunchMode expected_mode) {
    base::RunLoop run_loop;
    base::MockCallback<base::OnceCallback<void(std::optional<LaunchMode>)>>
        mock_callback;
    ON_CALL(mock_callback, Run(testing::_))
        .WillByDefault(
            [&run_loop](std::optional<LaunchMode>) { run_loop.Quit(); });
    EXPECT_CALL(mock_callback, Run(std::optional<LaunchMode>(expected_mode)))
        .WillOnce(testing::DoDefault());
    ComputeLaunchMode(cmd_line, mock_callback.Get());
    run_loop.Run();
  }

 private:
  base::test::TaskEnvironment env_;
};

TEST_F(LaunchModeRecorderTest, NoMetric) {
  base::HistogramTester histogram_tester;
  base::OnceCallback<void(std::optional<LaunchMode>)> record_callback =
      GetRecordLaunchModeForTesting();
  std::move(record_callback).Run(std::nullopt);
  histogram_tester.ExpectTotalCount(kLaunchModeMetric, 0);
}

TEST_F(LaunchModeRecorderTest, NoneMetric) {
  base::HistogramTester histogram_tester;
  base::OnceCallback<void(std::optional<LaunchMode>)> record_callback =
      GetRecordLaunchModeForTesting();
  std::move(record_callback).Run(LaunchMode::kNone);
  histogram_tester.ExpectTotalCount(kLaunchModeMetric, 0);
}

TEST_F(LaunchModeRecorderTest, SimpleMetric) {
  base::HistogramTester histogram_tester;
  base::OnceCallback<void(std::optional<LaunchMode>)> record_callback =
      GetRecordLaunchModeForTesting();
  std::move(record_callback).Run(LaunchMode::kWithUrl);
  histogram_tester.ExpectUniqueSample(kLaunchModeMetric, LaunchMode::kWithUrl,
                                      1);
}

#if !BUILDFLAG(IS_MAC)

TEST_F(LaunchModeRecorderTest, Other) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  ComputeLaunchModeAndVerify(cmd_line, LaunchMode::kOtherOS);
}

#else  // IS_MAC

// TODO(crbug.com/437351384): Flaky
TEST_F(LaunchModeRecorderTest, DISABLED_Mac) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  ComputeLaunchModeAndVerify(cmd_line, LaunchMode::kMacUndockedDiskLaunch);
}

#endif  // !BUILDFLAG(IS_MAC)
