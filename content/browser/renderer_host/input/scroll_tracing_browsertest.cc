// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/tracing/test_trace_processor.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_gesture_controller.h"
#include "content/common/input/synthetic_smooth_scroll_gesture.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"

namespace content {

class ScrollTracingBrowserTest : public ContentBrowserTest {
 public:
  ScrollTracingBrowserTest() {
    scoped_feature_list_.InitWithFeatures({ukm::kUkmFeature}, {});
  }

  ScrollTracingBrowserTest(const ScrollTracingBrowserTest&) = delete;
  ScrollTracingBrowserTest& operator=(const ScrollTracingBrowserTest&) = delete;

  ~ScrollTracingBrowserTest() override = default;

  void PreRunTestOnMainThread() override {
    ContentBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  RenderWidgetHostImpl* GetRenderWidgetHostImpl() {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    return root->current_frame_host()->GetRenderWidgetHost();
  }

  void DoScroll(gfx::Point starting_point,
                std::vector<gfx::Vector2d> distances,
                content::mojom::GestureSourceType source) {
    // Create and queue gestures
    for (const auto distance : distances) {
      SyntheticSmoothScrollGestureParams params;
      params.gesture_source_type = source;
      params.anchor = gfx::PointF(starting_point);
      params.distances.push_back(-distance);
      params.granularity = ui::ScrollGranularity::kScrollByPrecisePixel;
      auto gesture = std::make_unique<SyntheticSmoothScrollGesture>(params);

      base::RunLoop run_loop;
      GetRenderWidgetHostImpl()->QueueSyntheticGesture(
          std::move(gesture),
          base::BindLambdaForTesting([&](SyntheticGesture::Result result) {
            EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
            run_loop.Quit();
          }));
      run_loop.Run();

      // Update the previous start point.
      starting_point = gfx::Point(starting_point.x() + distance.x(),
                                  starting_point.y() + distance.y());
    }
  }

  void ValidateUkm(GURL url,
                   std::string_view entry_name,
                   std::map<std::string_view, int64_t> expected_values) {
    const auto& entries =
        test_ukm_recorder_->GetMergedEntriesByName(entry_name);
    EXPECT_EQ(1u, entries.size());
    for (const auto& kv : entries) {
      test_ukm_recorder_->ExpectEntrySourceHasUrl(kv.second.get(), url);
      for (const auto& expected_kv : expected_values) {
        EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(kv.second.get(),
                                                       expected_kv.first));
        if (*(test_ukm_recorder_->GetEntryMetric(kv.second.get(),
                                                 expected_kv.first)) != 0) {
          test_ukm_recorder_->ExpectEntryMetric(
              kv.second.get(), expected_kv.first,
              ukm::GetExponentialBucketMinForCounts1000(expected_kv.second));
        }
      }
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

std::optional<int64_t> ConvertToIntValue(std::string query_value) {
  int64_t result;
  if (base::StringToInt64(query_value, &result)) {
    return result;
  }
  return std::nullopt;
}

// NOTE:  Mac doesn't support touch events, and will not record scrolls with
// touch input. Linux bots are inconsistent.

}  // namespace content
