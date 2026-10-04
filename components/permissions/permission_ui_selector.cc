// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_ui_selector.h"

#include <optional>

namespace permissions {

PermissionUiSelector::Decision::Decision(
    std::optional<QuietUiReason> quiet_ui_reason,
    GeolocationAccuracy geolocation_accuracy)
    : quiet_ui_reason(quiet_ui_reason),
      geolocation_accuracy(geolocation_accuracy) {}
PermissionUiSelector::Decision::~Decision() = default;

PermissionUiSelector::Decision::Decision(const Decision&) = default;
PermissionUiSelector::Decision& PermissionUiSelector::Decision::operator=(
    const Decision&) = default;

bool PermissionUiSelector::Decision::operator==(const Decision&) const =
    default;

// static
PermissionUiSelector::Decision
PermissionUiSelector::Decision::UseNormalUiAndShowNoWarning() {
  return Decision::UseNormalUi();
}

// static
PermissionUiSelector::Decision PermissionUiSelector::Decision::UseNormalUi(
    GeolocationAccuracy geolocation_accuracy) {
  return Decision(std::nullopt, geolocation_accuracy);
}

// static
PermissionUiSelector::Decision PermissionUiSelector::Decision::UseQuietUi(
    QuietUiReason quiet_ui_reason) {
  return Decision(quiet_ui_reason, GeolocationAccuracy::kUnspecified);
}

}  // namespace permissions
