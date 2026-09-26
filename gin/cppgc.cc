// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/cppgc.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "gin/gin_features.h"
#include "gin/public/v8_platform.h"
#include "v8/include/cppgc/platform.h"

namespace gin {

namespace {

int g_init_count = 0;

}  // namespace

void InitializeCppgcFromV8Platform() {
  static constexpr size_t kCageSize =
      static_cast<size_t>(16) * 1024 * 1024 * 1024;

  DCHECK_GE(g_init_count, 0);
  if (g_init_count++ > 0) {
    return;
  }

  cppgc::InitializeProcess(gin::V8Platform::Get()->GetPageAllocator(),
                           kCageSize);
}

void MaybeShutdownCppgc() {
  DCHECK_GT(g_init_count, 0);
  if (--g_init_count > 0)
    return;

  cppgc::ShutdownProcess();
}

}  // namespace gin
