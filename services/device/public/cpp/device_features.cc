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

// Expose serial port logical connection state and dispatch connection events
// for Bluetooth serial ports when the Bluetooth device connection state
// changes.
BASE_FEATURE(kSerialPortConnected, base::FEATURE_ENABLED_BY_DEFAULT);

// Restricts the sharing of C++ SerialPort and WritableStream instances across
// different DOMWrapperWorld contexts to prevent cross-world leaks.
BASE_FEATURE(kWebSerialWorldIsolatedCache, base::FEATURE_ENABLED_BY_DEFAULT);

// Avoid triggering the macOS Bluetooth permission prompt when
// navigator.serial.getPorts() is called and permission is undetermined.
BASE_FEATURE(kAvoidBluetoothPromptInGetPorts, base::FEATURE_ENABLED_BY_DEFAULT);

// This feature allows to dynamically introduce an additional list of devices
// blocked by WebUSB via a Finch parameter. This parameter should be specified
// in the Finch configuration to manage the list of blocked devices.
BASE_FEATURE(kWebUsbBlocklist,
             "WebUSBBlocklist",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, WebUSB control transfers are blocked if they target a
// protected interface class, even if the recipient is not set to interface
// or endpoint. This protects devices which ignore this field.
BASE_FEATURE(kWebUsbProtectedClassControlTransferBlock,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, WebUSB control transfers enforce a positive matching allowlist
// for Standard requests (permitting only GET_STATUS, GET_DESCRIPTOR,
// GET_CONFIGURATION, GET_INTERFACE, SYNCH_FRAME). All other Standard requests
// are strictly blocked.
BASE_FEATURE(kWebUsbEnforceStandardRequestAllowlist,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, WebUSB rejects claiming interfaces that share endpoints with
// already claimed interfaces, and avoids overwriting endpoint mapping entries.
// See crbug.com/513167952.
BASE_FEATURE(kWebUsbHardenEndpointAliasing, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, accessing the navigator.hid attribute does not prevent the
// frame from entering the back forward cache.
BASE_FEATURE(kWebHidAttributeAllowsBackForwardCache,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Modifies the internal allowlist behavior that enables privileged extensions
// to bypass the HID blocklist when accessing FIDO devices. When enabled,
// privileged extensions can access non-FIDO interfaces on known security keys.
BASE_FEATURE(kSecurityKeyHidInterfacesAreFido,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables recursive filtering of nested HID collections to prevent WebHID
// security bypasses (e.g., nested keyboards or FIDO keys).
BASE_FEATURE(kWebHidRecursiveFiltering, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether Chrome will try to automatically detach kernel drivers when
// a USB interface is busy.
#if BUILDFLAG(IS_LINUX)
BASE_FEATURE(kAutomaticUsbDetach, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX)
// Controls whether we report the product name (like macOS and Win)
// over the HID_NAME in the WebHID API.
BASE_FEATURE(kProductNameOverHidName, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX)

// Splits DTR and RTS control signals. See crbug.com/420689824.
// Can be disabled as a kill switch if needed.
BASE_FEATURE(kSerialSplitDtrAndRts, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kHidReportRequestExactLength, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

}  // namespace features
