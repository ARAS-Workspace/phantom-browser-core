// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/common/allocation_info.h"

#include "base/debug/stack_trace.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include <pthread.h>
#endif

namespace gwp_asan::internal {

size_t AllocationInfo::GetStackTrace(base::span<const void*> trace) {
  // TODO(vtsyrklevich): Investigate using trace_event::CFIBacktraceAndroid
  // on 32-bit Android for canary/dev (where we can dynamically load unwind
  // data.)
  return base::debug::CollectStackTrace(trace);
}

}  // namespace gwp_asan::internal
