// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"

// static
const char QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation[] =
    "enable_adaptive_activation";

// static
const char
    QuietNotificationPermissionUiConfig::kEnableAdaptiveActivationDryRun[] =
        "enable_adaptive_activation_dry_run";

// static
const char QuietNotificationPermissionUiConfig::
    kAdaptiveActivationActionWindowSizeInDays[] =
        "adaptive_activation_windows_size_in_days";

// static
const char QuietNotificationPermissionUiConfig::kMiniInfobarExpandLinkText[] =
    "mini_infobar_expand_link_text";

// static
bool QuietNotificationPermissionUiConfig::IsAdaptiveActivationEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts, kEnableAdaptiveActivation,
      false /* default */);
}

// static
bool QuietNotificationPermissionUiConfig::IsAdaptiveActivationDryRunEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts, kEnableAdaptiveActivationDryRun,
      false /* default */);
}

// static
base::TimeDelta
QuietNotificationPermissionUiConfig::GetAdaptiveActivationWindowSize() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return base::Days(90);

  return base::Days(base::GetFieldTrialParamByFeatureAsInt(
      features::kQuietNotificationPrompts,
      kAdaptiveActivationActionWindowSizeInDays, 90 /* default */));
}

// static
QuietNotificationPermissionUiConfig::InfobarLinkTextVariation
QuietNotificationPermissionUiConfig::GetMiniInfobarExpandLinkText() {
  return base::GetFieldTrialParamByFeatureAsInt(
             features::kQuietNotificationPrompts, kMiniInfobarExpandLinkText, 0)
             ? InfobarLinkTextVariation::kManage
             : InfobarLinkTextVariation::kDetails;
}
