// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <string_view>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace {

void MaybeIncreaseTestTimeouts(int argc, char** argv) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // The minimum and the default value when unspecified is 45000.
  if (!command_line->HasSwitch(switches::kTestLauncherTimeout)) {
    command_line->AppendSwitchUTF8(switches::kTestLauncherTimeout, "90000");
  }

  // The minimum and the default value when unspecified is 30000.
  if (!command_line->HasSwitch(switches::kUiTestActionMaxTimeout)) {
    command_line->AppendSwitchUTF8(switches::kUiTestActionMaxTimeout, "45000");
  }

  // The minimum and the default value when unspecified is 10000.
  if (!command_line->HasSwitch(switches::kUiTestActionTimeout)) {
    command_line->AppendSwitchUTF8(switches::kUiTestActionTimeout, "40000");
  }
}

// Disable the fallback fetcher for the current process. This is achieved by
// adding `kNetWorkerSwitch` to the process command line to make it look like
// a net worker process.
void SkipFallbackNetworkFetcher() {
#if BUILDFLAG(IS_MAC)
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      updater::kNetWorkerSwitch);
#endif  // BUILDFLAG(IS_MAC)
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  absl::Cleanup reset_command_line = &base::CommandLine::Reset;

  // Change the test timeout defaults if the command line arguments to override
  // them are not present.
  MaybeIncreaseTestTimeouts(argc, argv);

  // To make setting up network response expectations easier in test, just
  // disable fallback fetcher altogether within this process.
  SkipFallbackNetworkFetcher();

  // Assume all test bots have the {ISOLATED_OUTDIR} environment variable set.
  // Otherwise, don't run branded updater tests on a developer's system because
  // doing so breaks the updater on the system.
  using std::operator""sv;
  if constexpr ("ChromiumUpdater"sv.compare(PRODUCT_FULLNAME_STRING)) {
    if (!std::getenv("ISOLATED_OUTDIR")) {
      LOG(ERROR)
          << "Running branded updater tests breaks the updater for "
             "the branded browser. This is unavoidable in the current "
             "implementation. If you don't care about broken updaters and "
             "want to run the branded updater tests locally, define an "
             "environment variable ISOLATED_OUTDIR and set it to a local "
             "directory.";
      return -1;
    }
  }

  // Use the {ISOLATED_OUTDIR} as a log destination for the test suite.
  base::TestSuite test_suite(argc, argv);
  updater::test::InitLoggingForUnitTest(base::FilePath([] {
    switch (updater::GetUpdaterScopeForTesting()) {
      case updater::UpdaterScope::kSystem:
        return FILE_PATH_LITERAL("updater_test_system.log");
      case updater::UpdaterScope::kUser:
        return FILE_PATH_LITERAL("updater_test.log");
    }
  }()));
  chrome::RegisterPathProvider();
  return base::LaunchUnitTestsWithOptions(
      argc, argv, 1, 10, true, base::BindRepeating([] {
        LOG(ERROR) << "A test timeout has occured in "
                   << updater::test::GetTestName();
        updater::test::CreateIntegrationTestCommands()->PrintLog();
      }),
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
