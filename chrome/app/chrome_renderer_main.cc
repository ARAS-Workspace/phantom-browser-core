// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/command_line.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/app/llvm_profile_util.h"
#include "content/public/app/content_main.h"

extern "C" {

#if BUILDFLAG(IS_POSIX)
[[gnu::visibility("default")]] int ChromeRendererMain(int argc,
                                                      const char** argv) {
  SetLLVMProfileProcessType(ProfileProcessType::kRenderer);
  ChromeMainDelegate chrome_main_delegate(
      {.exe_entry_point_ticks = base::TimeTicks::Now()});
  content::ContentMainParams params(&chrome_main_delegate);
  params.argc = argc;
  params.argv = argv;
  base::CommandLine::Init(params.argc, params.argv);
  base::PoissonAllocationSampler::Init();
  return content::ContentMain(std::move(params));
}
#endif

}  // extern "C"

#if BUILDFLAG(IS_POSIX)
// TODO(crbug.com/534570563): Do not define `main` once this becomes a
// shared_library on macOS.
int main(int argc, const char** argv) {
  return ChromeRendererMain(argc, argv);
}
#endif
