// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/core_unwinders.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/profiler/frame_pointer_unwinder.h"
#include "build/build_config.h"

namespace base {

StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory() {
  return StackSamplingProfiler::UnwindersFactory();
}

}  // namespace base
