// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/debug_info_printer.h"

#include <string_view>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/time/time.h"
#if BUILDFLAG(IS_MAC)
#include <inttypes.h>

#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#endif

namespace web_app::test {
namespace {
constexpr std::string_view kDisableLogDebugInfoToConsole =
    "disable-web-app-internals-log";
}  // namespace

void LogDebugInfoToConsole(const std::vector<Profile*>& profiles,
                           base::TimeDelta time_ago_for_system_log_capture) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableLogDebugInfoToConsole)) {
    return;
  }

  // On Mac OS also include system log output, as that is the only place logs
  // from app shims would end up. Do note that this log will include messages
  // from all tests that were running at the time, not just this test.
#if BUILDFLAG(IS_MAC)
  std::vector<std::string> log_argv = {
      "log",
      "show",
      "--process",
      "app_mode_loader",
      "--last",
      base::StringPrintf("%" PRId64 "s",
                         time_ago_for_system_log_capture.InSeconds() + 1)};
  std::string log_output;
  base::GetAppOutputAndError(log_argv, &log_output);
  LOG(INFO) << "System logs during this test run (could include other tests):\n"
            << log_output;
#endif
}

}  // namespace web_app::test
