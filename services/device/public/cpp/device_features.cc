// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/device_features.h"

#include "build/build_config.h"

namespace features {

// Enables an extra set of concrete sensors classes based on Generic Sensor API,
// which expose previously unexposed platform features, e.g. ALS or Magnetometer
BASE_FEATURE(kGenericSensorExtraClasses, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables an Allow/Ask/Block set of default permissions for sensors.
BASE_FEATURE(kSensorsAllowAskBlockPermissionModel,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables browser-side privacy mitigations for generic sensors.
BASE_FEATURE(kSensorPrivacyMitigations, base::FEATURE_ENABLED_BY_DEFAULT);

// Modifies the internal allowlist behavior that enables privileged extensions
// to bypass the HID blocklist when accessing FIDO devices. When enabled,
// privileged extensions can access non-FIDO interfaces on known security keys.
BASE_FEATURE(kSecurityKeyHidInterfacesAreFido,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables recursive filtering of nested HID collections to prevent WebHID
// security bypasses (e.g., nested keyboards or FIDO keys).
BASE_FEATURE(kWebHidRecursiveFiltering, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX)
// Controls whether we report the product name (like macOS and Win)
// over the HID_NAME in the WebHID API.
BASE_FEATURE(kProductNameOverHidName, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kHidReportRequestExactLength, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

}  // namespace features
