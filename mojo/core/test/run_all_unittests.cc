// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/build_config.h"
#include "mojo/core/test/mojo_test_suite_base.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
  // Silence death test thread warnings on Linux. We can afford to run our death
  // tests a little more slowly (< 10 ms per death test on a Z620).
  // On android, we need to run in the default mode, as the threadsafe mode
  // relies on execve which is not available.
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  mojo::core::test::MojoTestSuiteBase test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
