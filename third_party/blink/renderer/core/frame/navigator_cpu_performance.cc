// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_cpu_performance.h"

#include "third_party/blink/public/mojom/cpu_performance.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

// static
uint16_t NavigatorCPUPerformance::cpuPerformance(Navigator& navigator) {
  return 0;
}

}  // namespace blink
