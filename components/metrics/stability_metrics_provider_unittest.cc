// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

class StabilityMetricsProviderTest : public testing::Test {
 public:
  StabilityMetricsProviderTest() {
    StabilityMetricsProvider::RegisterPrefs(prefs_.registry());
  }

  StabilityMetricsProviderTest(const StabilityMetricsProviderTest&) = delete;
  StabilityMetricsProviderTest& operator=(const StabilityMetricsProviderTest&) =
      delete;

  ~StabilityMetricsProviderTest() override = default;

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(StabilityMetricsProviderTest, ProvideStabilityMetrics) {
  base::HistogramTester histogram_tester;
  StabilityMetricsProvider stability_provider(&prefs_);
  MetricsProvider* provider = &stability_provider;
  SystemProfileProto system_profile;
  provider->ProvideStabilityMetrics(&system_profile);

#if BUILDFLAG(IS_ANDROID)
  // Initial log metrics: only expected if non-zero.
  const SystemProfileProto_Stability& stability = system_profile.stability();
  // The launch count field is used on Android only.
  EXPECT_FALSE(stability.has_launch_count());
#endif

  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kLaunch, 0);
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kBrowserCrash, 0);
}

TEST_F(StabilityMetricsProviderTest, RecordStabilityMetrics) {
  base::HistogramTester histogram_tester;
  {
    StabilityMetricsProvider recorder(&prefs_);
    recorder.LogLaunch();
    recorder.LogCrash(base::Time());
  }

  {
    StabilityMetricsProvider stability_provider(&prefs_);
    MetricsProvider* provider = &stability_provider;
    SystemProfileProto system_profile;
    provider->ProvideStabilityMetrics(&system_profile);

#if BUILDFLAG(IS_ANDROID)
    // Initial log metrics: only expected if non-zero.
    const SystemProfileProto_Stability& stability = system_profile.stability();
    // The launch count field is populated only on Android.
    EXPECT_EQ(1, stability.launch_count());
#endif

    histogram_tester.ExpectBucketCount("Stability.Counts2",
                                       StabilityEventType::kLaunch, 1);
    histogram_tester.ExpectBucketCount("Stability.Counts2",
                                       StabilityEventType::kBrowserCrash, 1);
  }
}

}  // namespace metrics
