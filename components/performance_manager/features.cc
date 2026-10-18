// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#include "components/performance_manager/public/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace performance_manager::features {

BASE_FEATURE(kPerformanceInterventionDemoMode,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPerformanceInterventionNotificationImprovements,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kMinimumTimeBetweenReshow,
                   &kPerformanceInterventionNotificationImprovements,
                   "minimum_time_reshow",
                   base::Hours(1));

BASE_FEATURE_PARAM(int,
                   kAcceptanceRateWindowSize,
                   &kPerformanceInterventionNotificationImprovements,
                   "window_size",
                   10);

BASE_FEATURE_PARAM(int,
                   kScaleMaxTimesPerDay,
                   &kPerformanceInterventionNotificationImprovements,
                   "scale_max_times_per_day",
                   3);

BASE_FEATURE_PARAM(int,
                   kScaleMaxTimesPerWeek,
                   &kPerformanceInterventionNotificationImprovements,
                   "scale_max_times_per_week",
                   21);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kNoAcceptanceBackOff,
                   &kPerformanceInterventionNotificationImprovements,
                   "no_acceptance_back_off",
                   base::Days(7));

BASE_FEATURE(kPerformanceInterventionNotificationStringImprovements,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kNotificationStringVersion,
                   &kPerformanceInterventionNotificationStringImprovements,
                   "string_version",
                   1);

BASE_FEATURE(kLevelDBSiteDataStoreBestEffort,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableBestEffortTaskInhibitingPolicy,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBestEffortTaskInhibitingPeriod,
                   &kEnableBestEffortTaskInhibitingPolicy,
                   "enable_best_effort_task_inhibiting_period",
                   base::Minutes(5));
BASE_FEATURE_PARAM(
    base::TimeDelta,
    kBestEffortTaskInhibitingMinimumAllowedTimePerPeriod,
    &kEnableBestEffortTaskInhibitingPolicy,
    "enable_best_effort_task_inhibiting_minimum_allowed_time_per_period",
    base::Seconds(30));

BASE_FEATURE(kPMProcessPriorityPolicy, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kInheritParentPriority{
    &kPMProcessPriorityPolicy, "inherit_parent_priority", true};

const base::FeatureParam<bool> kRenderedOutOfViewIsNotVisible{
    &kPMProcessPriorityPolicy, "rendered_out_of_view_is_not_visible", false};

const base::FeatureParam<bool> kRendererHighInitialPriority{
    &kPMProcessPriorityPolicy, "renderer_high_initial_priority", false};

BASE_FEATURE(kPMLoadingPageVoter, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUrgentPageDiscarding, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCPUMeasurementInFreezingPolicy, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMemoryMeasurementInFreezingPolicy,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDiscardFrozenBrowsingInstancesWithGrowingPMF,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Note: These params are associated with `kCPUMeasurementInFreezingPolicy`
// instead of `kDiscardFrozenBrowsingInstancesWithGrowingPMF`,
// to allow retrieving the value without activating these two features.
BASE_FEATURE_PARAM(int,
                   kFreezingMemoryGrowthThresholdToDiscardKb,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_memory_growth_threshold_to_discard_kb",
                   /* 100 MB */ 100 * 1024);

BASE_FEATURE_PARAM(double,
                   kFreezingHighCPUProportion,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_high_cpu_proportion",
                   0.05);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFreezingVisibleProtectionTime,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_visible_protection_time",
                   base::Minutes(5));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFreezingAudioProtectionTime,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_audio_protection_time",
                   base::Minutes(5));

BASE_FEATURE(kFreezingFollowsDiscardOptOut, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRecordFreezingEligibilityUKM, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kInfiniteTabsFreezing, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kInfiniteTabsFreezing_NumProtectedTabs,
                   &kInfiniteTabsFreezing,
                   "num_protected_tabs",
                   5);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kInfiniteTabsFreezing_UnfreezeInterval,
                   &kInfiniteTabsFreezing,
                   "unfreeze_interval",
                   base::Minutes(1));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kInfiniteTabsFreezing_UnfreezeDuration,
                   &kInfiniteTabsFreezing,
                   "unfreeze_duration",
                   base::Seconds(5));

BASE_FEATURE(kInfiniteTabsFreezingOnMemoryPressure,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kInfiniteTabsFreezingOnMemoryPressureInterval,
                   &kInfiniteTabsFreezingOnMemoryPressure,
                   "interval",
                   base::Seconds(5));

BASE_FEATURE_PARAM(int,
                   kInfiniteTabsFreezingOnMemoryPressurePercent,
                   &kInfiniteTabsFreezingOnMemoryPressure,
                   "percent_threshold",
                   15);

BASE_FEATURE(kDiscardOnCommitLimit, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kDiscardOnCommitLimit_MinAvailablePercent,
                   &kDiscardOnCommitLimit,
                   "min_available_percent",
                   10);

BASE_FEATURE(kResourceAttributionIncludeOrigins,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSeamlessRenderFrameSwap, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUnimportantFramesPriority, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThrottleUnimportantFrameRate, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kKeepDefaultSearchEngineRendererAlive,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBoostClosingTabs, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceForegroundPriorityForAllTabs,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Defines the feature to enable this policy.
BASE_FEATURE(kTransientKeepAlivePolicy, base::FEATURE_DISABLED_BY_DEFAULT);

// Defines the Finch parameter for the keep-alive duration.
// Default is 23 seconds, which represents the threshold at which approximately
// 50% of renderer process could potentially be reused.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kTransientKeepAlivePolicyDuration,
                   &kTransientKeepAlivePolicy,
                   "duration",
                   base::Seconds(23));

// Defines the Finch parameter for the maximum number of empty renderer
// processes to keep alive simultaneously. Default is 10.
BASE_FEATURE_PARAM(size_t,
                   kTransientKeepAlivePolicyMaxCount,
                   &kTransientKeepAlivePolicy,
                   "count",
                   10);

BASE_FEATURE(kExtensionServiceWorkerVoter, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, this feature prevents the browser from proactively discarding
// tabs to save memory. This is used to measure the impact of tab discarding on
// memory usage and user experience compared to other memory saving features.
BASE_FEATURE(kDisableTabDiscarding, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseLoadingStateToDetectBackgroundTitleOrFaviconUpdate,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIgnoreMediaQueryFaviconUpdates, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace performance_manager::features
