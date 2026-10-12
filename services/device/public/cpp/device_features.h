// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the
// services/device module.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_
#define SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "services/device/public/cpp/device_features_export.h"

namespace features {

// The features should be documented alongside the definition of their values
// in the .cc file.
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kGenericSensorExtraClasses);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kSensorsAllowAskBlockPermissionModel);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kSensorPrivacyMitigations);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kSerialPortConnected);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kWebSerialWorldIsolatedCache);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kAvoidBluetoothPromptInGetPorts);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kWebUsbBlocklist);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kWebUsbProtectedClassControlTransferBlock);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kWebUsbHardenEndpointAliasing);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kWebUsbEnforceStandardRequestAllowlist);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kWebHidAttributeAllowsBackForwardCache);

DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kSecurityKeyHidInterfacesAreFido);

DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kWebHidRecursiveFiltering);

#if BUILDFLAG(IS_LINUX)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kAutomaticUsbDetach);
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kProductNameOverHidName);
#endif  // BUILDFLAG(IS_LINUX)

DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kSerialSplitDtrAndRts);

#if BUILDFLAG(IS_MAC)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kHidReportRequestExactLength);
#endif  // BUILDFLAG(IS_MAC)

}  // namespace features

#endif  // SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_
