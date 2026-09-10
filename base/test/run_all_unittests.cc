// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/process/set_process_title_linux.h"
#endif

namespace base {

namespace {

class BaseUnittestSuite : public TestSuite {
 public:
  BaseUnittestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    TestSuite::Initialize();

  }
};

}  // namespace

}  // namespace base

int main(int argc, char** argv) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // For setproctitle unit tests.
  setproctitle_init(const_cast<const char**>(argv));
#endif

  base::BaseUnittestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
