// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_debug_util.h"

#include "base/check.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

namespace debug {
XrResult GetCurrentXrTime(const XrInstance& instance_,
                          const OpenXrExtensionHelper& extension_helper,
                          XrTime* current_time) {
  DCHECK(current_time);
  return XR_ERROR_FUNCTION_UNSUPPORTED;
}
}  // namespace debug

}  // namespace device
