// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/features.h"

#include "build/build_config.h"

namespace device {

namespace features {

#if BUILDFLAG(IS_LINUX)
// Controls whether Web Bluetooth should support confirm-only and confirm-PIN
// pairing mode on Linux
BASE_FEATURE(kWebBluetoothConfirmPairingSupport,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID)
// Controls whether to override LocationRequest parameters in
// LocationProviderGmsCore
BASE_FEATURE(kGmsCoreLocationRequestParamOverride,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to fail closed and report a position error when a precise
// location leak is detected in LocationProviderGmsCore.
BASE_FEATURE(kGmsCoreFailClosedOnPreciseLeak, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
}  // namespace device
