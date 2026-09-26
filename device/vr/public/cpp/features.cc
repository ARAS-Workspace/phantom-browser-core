// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/public/cpp/features.h"

#include "base/feature_list.h"
#include "device/vr/buildflags/buildflags.h"

namespace device::features {
// Enables rendering to WebXR sessions with the WebGPU API.
BASE_FEATURE(kWebXRWebGPUBinding, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to experimental WebXR features.
BASE_FEATURE(kWebXRIncubations, base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag for the WebXRInternals debugging page.
BASE_FEATURE(kWebXrInternals, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to WebXR composition layers.
BASE_FEATURE(kWebXRLayers, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the orientation sensor based device is enabled.
BASE_FEATURE(kWebXROrientationSensorDevice,
             // TODO(crbug.com/529477337): Restrict this feature to Android.
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to the WebXR plane-detection feature
BASE_FEATURE(kWebXRPlaneDetection, base::FEATURE_ENABLED_BY_DEFAULT);

// Allows blink to process the `visible-blurred` state.
BASE_FEATURE(kWebXrVisibleBlurred, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_OPENXR)
// Controls WebXR support for the OpenXR Runtime.
BASE_FEATURE(kOpenXR, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the spatial entities framework is allowed to use depth-based
// hit tests or only plane-based ones.
BASE_FEATURE(kSpatialEntitesDepthHitTest, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOpenXrEnabled() {
  static bool is_xr_device = IsXrDevice();
  return base::FeatureList::IsEnabled(kOpenXR) || is_xr_device;
}
#endif  // ENABLE_OPENXR

bool IsXrDevice() {
  return false;
}

bool IsHandTrackingEnabled() {
#if BUILDFLAG(ENABLE_OPENXR)
  return IsOpenXrEnabled();
#else
  return false;
#endif
}
}  // namespace device::features
