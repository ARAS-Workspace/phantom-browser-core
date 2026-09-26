// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/performance_hint_utils.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "build/build_config.h"

namespace viz {

bool CheckThreadIdsDoNotBelongToCurrentProcess(
    const base::flat_set<base::PlatformThreadId>&
        thread_ids_from_sandboxed_process) {
  return false;
}

}  // namespace viz
