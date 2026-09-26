// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "battery_saver.h"

#include <optional>

#include "base/check_is_test.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

namespace {
std::optional<bool> g_override_battery_saver_mode_for_testing;
}  // namespace

namespace battery {

void OverrideIsBatterySaverEnabledForTesting(bool is_battery_saver_mode) {
  g_override_battery_saver_mode_for_testing = is_battery_saver_mode;
}

void ResetIsBatterySaverEnabledForTesting() {
  g_override_battery_saver_mode_for_testing.reset();
}

bool IsBatterySaverEnabled() {
  if (g_override_battery_saver_mode_for_testing.has_value()) {
    CHECK_IS_TEST();
    return g_override_battery_saver_mode_for_testing.value();
  }
  return false;
}

}  // namespace battery
