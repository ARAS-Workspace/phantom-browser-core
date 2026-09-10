// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "content/public/common/content_switches.h"
#include "ui/compositor/compositor_switches.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs(/*cores_per_job=*/2);
  if (parallel_jobs == 0U) {
    return 1;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Adjust switches for interactive tests where the user is expected to
  // manually verify results.
  if (command_line->HasSwitch(switches::kTestLauncherInteractive)) {
    // Since the test is interactive, the invoker will want to have pixel output
    // to actually see the result.
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  }

  ChromeTestSuiteRunner runner;
  ChromeTestLauncherDelegate delegate(&runner);
  return LaunchChromeTests(parallel_jobs, &delegate, argc, argv);
}
