// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace send_tab_to_self {

BASE_FEATURE(kSendTabToSelfEnableNotificationTimeOut,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendTabToSelfPropagateFormFields,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendTabToSelfPropagateScrollPosition,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendTabToSelfImprovedLastActiveLabels,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendTabToSelfPropagateNavigationHistory,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendTabToSelfAutoOpen, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSendTabToSelfSupportAutoOpenInTabGrid,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSendTabToSelfEnhancedDesktopUI,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendTabToSelfPostSendToast, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendTabToSelfExtraEntryPoints, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendTabToSelfEnhancedDesktopUIv2,
             "SendTabToSelfEnhancedDesktopUIv2",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSendTabToSelfGesture, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendTabToSelfOpenNativeApp, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSendTabToSelfEnhancedBottomsheet,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSendTabToSelfDynamicShortcuts, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace send_tab_to_self
