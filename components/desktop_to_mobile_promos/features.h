// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_
#define COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_

#include "base/containers/enum_set.h"

// Enum to represent promo types of feature kMobilePromoOnDesktop.
// LINT.IfChange(MobilePromoOnDesktopPromoType)
enum class MobilePromoOnDesktopPromoType {
  kMinValue = 0,
  // Indicates that all types of promos are enabled for the device.
  kAllPromos = kMinValue,
  kLensPromo = 1,
  kESBPromo = 2,
  kAutofillPromo = 3,
  kTabGroups = 4,
  kPriceTracking = 5,
  kMaxValue = kPriceTracking,
};
// LINT.ThenChange(/components/sync/protocol/sync_enums.proto:MobilePromoOnDesktopPromoType)

// Represents a set of MobilePromoOnDesktopPromoType values.
// Used primarily to sync the specific types of promos an iOS device is
// eligible to receive to Desktop via DeviceInfo.
using MobilePromoOnDesktopPromoTypeSet =
    base::EnumSet<MobilePromoOnDesktopPromoType,
                  MobilePromoOnDesktopPromoType::kMinValue,
                  MobilePromoOnDesktopPromoType::kMaxValue>;

#endif  // COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_
