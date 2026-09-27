// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TIPS_CORE_TIPS_UTILS_H_
#define CHROME_BROWSER_TIPS_CORE_TIPS_UTILS_H_

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/tips/core/tips_types.h"

namespace tips {

// Constructs and returns the NotificationData object for the requested feature.
// This mainly contains UI information and an enum representing the feature
// type. |feature_type| the feature in question to create a data object for.
notifications::NotificationData GetTipsNotificationData(
    TipsNotificationsFeatureType feature_type);

}  // namespace tips

#endif  // CHROME_BROWSER_TIPS_CORE_TIPS_UTILS_H_
