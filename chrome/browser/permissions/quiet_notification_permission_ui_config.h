// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_
#define CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_

#include "build/build_config.h"

namespace base {
class TimeDelta;
}

// Field trial configuration for the quiet notification permission request UI.
class QuietNotificationPermissionUiConfig {
 public:
  enum class InfobarLinkTextVariation { kDetails = 0, kManage = 1 };

  // Name of the boolean variation parameter that determines if the quiet
  // notification permission prompt UI should be enabled adaptively after three
  // consecutive prompt denies.
  static const char kEnableAdaptiveActivation[];

  // Name of the boolean variation parameter that determines if the adaptive
  // activation quiet UI dry run study is enabled.
  static const char kEnableAdaptiveActivationDryRun[];

  // Name of the integer variation parameter that determines history windows
  // size in days in which 3 consecutive denies should be monitored.
  static const char kAdaptiveActivationActionWindowSizeInDays[];

  // Name of the variation parameter that determines which experimental string
  // to use for the link in the mini infobar in Android, which upon being
  // clicked, expands the mini infobar to show more options.
  static const char kMiniInfobarExpandLinkText[];

  // Whether or not adaptive activation is enabled. Adaptive activation means
  // that quiet notifications permission prompts will be turned on after three
  // consecutive prompt denies.
  static bool IsAdaptiveActivationEnabled();

  // Whether or not adaptive activation dry run is enabled. Adaptive activation
  // dry run means that UKM `Permission` events will be annotated with the
  // `SatisfiedAdaptiveTriggers` metric indicating whether the user had three
  // consecutive prompt denies.
  static bool IsAdaptiveActivationDryRunEnabled();

  // How long the window extends into the past, in which the user needs to make
  // 3 consecutive permission denies.
  static base::TimeDelta GetAdaptiveActivationWindowSize();

  // The text of the link to be shown in the mini infobar in Android.
  static InfobarLinkTextVariation GetMiniInfobarExpandLinkText();
};

#endif  // CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_
