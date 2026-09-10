// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main.h"

#include <stdint.h>

#include <iostream>
#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/time/time.h"
#include "build/build_config.h"
#if !defined(BUILDING_CHROME_RENDERER)
#include "chrome/app/chrome_main_delegate.h"
#endif
#include "chrome/app/startup_timestamps.h"
#include "chrome/browser/headless/headless_mode_init.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "partition_alloc/buildflags.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/app/chrome_main_mac.h"
#include "chrome/common/mac/detect_inappropriate_exit.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "base/base_switches.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/app/chrome_main_linux.h"
#endif

namespace {

// Returns storage to hold the browser process's initial command line.
std::optional<base::CommandLine>& GetInitialCommandLineStorage() {
  static base::NoDestructor<std::optional<base::CommandLine>>
      initial_command_line;
  return *initial_command_line;
}

}  // namespace

const base::CommandLine& GetInitialBrowserCommandLine() {
  // Will `CHECK` if called without a previous assignment during browser
  // process startup.
  return GetInitialCommandLineStorage().value();
}

#if BUILDFLAG(IS_POSIX)
extern "C" {
// This function must be marked with NO_STACK_PROTECTOR or it may crash on
// return, see the --change-stack-guard-on-fork command line flag.
NO_STACK_PROTECTOR __attribute__((visibility("default"))) int ChromeMain(
    int argc,
    const char** argv);
}
#else
#error Unknown platform.
#endif

#if BUILDFLAG(IS_POSIX)
int ChromeMain(int argc, const char** argv) {
#else
#error Unknown platform.
#endif

#if BUILDFLAG(IS_LINUX)
  PossiblyDetermineFallbackChromeChannel(argv[0]);
#endif

  ChromeMainDelegate chrome_main_delegate(
      {.exe_entry_point_ticks = base::TimeTicks::Now()});
  content::ContentMainParams params(&chrome_main_delegate);

  params.argc = argc;
  params.argv = argv;
  base::CommandLine::Init(params.argc, params.argv);

  base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());

  // Capture the unpolluted command line snapshot in the browser process.
  // This must happen immediately after CommandLine::Init to ensure we capture
  // the state before any internal programmatic mutations.
  if (!command_line->HasSwitch(switches::kProcessType)) {
    GetInitialCommandLineStorage() = *command_line;
  }

#if BUILDFLAG(IS_MAC)
  chrome::InitializeExitSixtyNineDetector();
  SetUpBundleOverrides();
#endif

#if BUILDFLAG(IS_LINUX)
  AppendExtraArgumentsToCommandLine(command_line);
#endif

  // PoissonAllocationSampler's TLS slots need to be set up before
  // MainThreadStackSamplingProfiler (in ChromeMainDelegate::ThreadPoolCreated),
  // which can allocate TLS slots of its own. On some platforms pthreads can
  // malloc internally to access higher-numbered TLS slots, which can cause
  // reentry in the heap profiler. (See the comment on
  // ReentryGuard::InitTLSSlot().)
  // TODO(crbug.com/40062835): Clean up other paths that call this Init()
  // function, which are now redundant.
  base::PoissonAllocationSampler::Init();

  // Chrome-specific process modes.
  std::unique_ptr<headless::HeadlessModeHandle> headless_mode_handle;
  if (headless::IsHeadlessMode()) {
    if (command_line->GetArgs().size() > 1) {
      LOG(ERROR) << "Multiple targets are not supported in headless mode.";
      return CHROME_RESULT_CODE_UNSUPPORTED_PARAM;
    }

    auto init_headless_mode = headless::InitHeadlessMode();
    if (!init_headless_mode.has_value()) {
      LOG(ERROR) << init_headless_mode.error();
      return EXIT_FAILURE;
    }

    headless_mode_handle = std::move(init_headless_mode.value());
  }

#if BUILDFLAG(IS_MAC)
  // Gracefully exit if a helper app was launched in an unexpected situation.
  if (IsHelperAppLaunchedBySystemOrThirdPartyApplication()) {
    return 0;
  }
#endif

  int rv = content::ContentMain(std::move(params));

  if (IsNormalResultCode(static_cast<ResultCode>(rv))) {
    return content::RESULT_CODE_NORMAL_EXIT;
  }
  return rv;
}
