// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_background_tracing_metrics_provider.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/background_tracing.h"
#include "content/public/test/browser_task_environment.h"
#include "services/tracing/public/cpp/background_tracing/background_tracing_manager.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/trace_log.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tracing {
namespace {

const char kDummyTrace[] = "Trace bytes as serialized proto";

class TestBackgroundTracingHelper
    : public tracing::BackgroundTracingManager::EnabledStateTestObserver {
 public:
  TestBackgroundTracingHelper() {
    tracing::BackgroundTracingManager::GetInstance()
        .AddEnabledStateObserverForTesting(this);
  }

  ~TestBackgroundTracingHelper() {
    tracing::BackgroundTracingManager::GetInstance()
        .RemoveEnabledStateObserverForTesting(this);
  }

  void OnTraceSaved() override { wait_for_trace_saved_.Quit(); }

  void WaitForTraceSaved() { wait_for_trace_saved_.Run(); }

 private:
  base::RunLoop wait_for_trace_saved_;
};

}  // namespace

class ChromeBackgroundTracingMetricsProviderTest : public testing::Test {
 public:
  ChromeBackgroundTracingMetricsProviderTest()
      : background_tracing_manager_(
            content::CreateBackgroundTracingManager(&tracing_delegate_)) {}

 private:
  content::BrowserTaskEnvironment task_environment_;
  ChromeTracingDelegate tracing_delegate_;
  std::unique_ptr<tracing::BackgroundTracingManager>
      background_tracing_manager_;
};

TEST_F(ChromeBackgroundTracingMetricsProviderTest, NoTraceData) {
  ChromeBackgroundTracingMetricsProvider provider(nullptr);
  ASSERT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(ChromeBackgroundTracingMetricsProviderTest, UploadsTraceLog) {
  TestBackgroundTracingHelper background_tracing_helper;
  ChromeBackgroundTracingMetricsProvider provider(nullptr);
  EXPECT_FALSE(provider.HasIndependentMetrics());

  tracing::BackgroundTracingManager::GetInstance().SaveTraceForTesting(
      kDummyTrace, "test_scenario", "test_rule", base::Token::CreateRandom());
  background_tracing_helper.WaitForTraceSaved();

  EXPECT_TRUE(provider.HasIndependentMetrics());
  metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(100);
  uma_proto.set_session_id(15);

  base::RunLoop run_loop;
  provider.ProvideIndependentMetrics(
      base::DoNothing(), base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }),
      &uma_proto,
      /* snapshot_manager=*/nullptr);
  run_loop.Run();

  EXPECT_EQ(100u, uma_proto.client_id());
  EXPECT_EQ(15, uma_proto.session_id());
  ASSERT_EQ(1, uma_proto.trace_log_size());
  EXPECT_EQ(metrics::TraceLog::COMPRESSION_TYPE_ZLIB,
            uma_proto.trace_log(0).compression_type());
  std::string serialize_trace;
  ASSERT_TRUE(compression::GzipUncompress(uma_proto.trace_log(0).raw_data(),
                                          &serialize_trace));
  EXPECT_EQ(kDummyTrace, serialize_trace);

  EXPECT_FALSE(provider.HasIndependentMetrics());
}

TEST_F(ChromeBackgroundTracingMetricsProviderTest, HandleMissingTrace) {
  ChromeBackgroundTracingMetricsProvider provider(nullptr);
  EXPECT_FALSE(provider.HasIndependentMetrics());

  metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(100);
  uma_proto.set_session_id(15);
  provider.ProvideIndependentMetrics(
      base::DoNothing(),
      base::BindOnce([](bool success) { EXPECT_FALSE(success); }), &uma_proto,
      /* snapshot_manager=*/nullptr);

  EXPECT_EQ(100u, uma_proto.client_id());
  EXPECT_EQ(15, uma_proto.session_id());
  EXPECT_EQ(0, uma_proto.trace_log_size());
  EXPECT_FALSE(provider.HasIndependentMetrics());
}

}  // namespace tracing
