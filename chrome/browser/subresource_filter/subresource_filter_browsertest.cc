// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "url/gurl.h"

namespace subresource_filter {

using subresource_filter::testing::TestRulesetPair;

namespace {

// The path to a multi-frame document used for tests.
static constexpr const char kTestFrameSetPath[] =
    "/subresource_filter/frame_set.html";

}  // namespace

// Tests -----------------------------------------------------------------------

// Disable the test as it's flaky on Win7 dbg.
// crbug.com/40125372
#define MAYBE_RendererDebugURL_NoLeakedThrottlePtrs \
  RendererDebugURL_NoLeakedThrottlePtrs

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       MAYBE_RendererDebugURL_NoLeakedThrottlePtrs) {
  // Allow crashes caused by the navigation to kChromeUICrashURL below.
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes(
      browser()->tab_strip_model()->GetActiveWebContents());

  // We have checks in the throttle manager that we don't improperly leak
  // activation state throttles. It would be nice to test things directly but it
  // isn't very feasible right now without exposing a bunch of internal guts of
  // the throttle manager.
  //
  // This test should crash the *browser process* with CHECK failures if the
  // component is faulty. The CHECK assumes that the crash URL and other
  // renderer debug URLs do not create a navigation throttle. See
  // crbug.com/40527486.
  content::RenderProcessHostWatcher crash_observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  browser()->OpenURL(content::OpenURLParams(GURL(blink::kChromeUICrashURL),
                                            content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});
  crash_observer.Wait();
}

// Test that a frame with an aborted initial load due to a frame deletion does
// not cause a crash.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       FrameDeletedDuringLoad_DoesNotCrash) {
  // Watches for title set by end of frame deletion script.
  content::TitleWatcher title_watcher(web_contents(), u"done");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/subresource_filter/delete_loading_frame.html")));

  // Wait for the script to complete.
  EXPECT_EQ(u"done", title_watcher.WaitAndGetTitle());
}

// Tests checking how histograms are recorded. ---------------------------------

class SubresourceFilterBrowserTestWithoutAdTagging
    : public SubresourceFilterBrowserTest {
 public:
  SubresourceFilterBrowserTestWithoutAdTagging() {
    feature_list_.InitAndDisableFeature(kAdTagging);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// This test only makes sense when AdTagging is disabled.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTestWithoutAdTagging,
                       ExpectHistogramsNotRecordedWhenFilteringNotActivated) {
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  ResetConfigurationToEnableOnPhishingSites(true /* measure_performance */);

  const GURL url = GetTestUrl(kTestFrameSetPath);
  // Note: The |url| is not configured to be fishing.

  base::HistogramTester tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // The following histograms are generated only when filtering is activated.
  tester.ExpectTotalCount(kSubresourceLoadsTotalForPage, 0);
  tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForPage, 0);
  tester.ExpectTotalCount(kSubresourceLoadsMatchedRulesForPage, 0);
  tester.ExpectTotalCount(kSubresourceLoadsDisallowedForPage, 0);
  tester.ExpectTotalCount(kEvaluationTotalWallDurationForPage, 0);
  tester.ExpectTotalCount(kEvaluationTotalCPUDurationForPage, 0);

  // The rest is produced by renderers, therefore needs to be merged here.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // But they still should not be recorded as the filtering is not activated.
  tester.ExpectTotalCount(kEvaluationWallDuration, 0);
  tester.ExpectTotalCount(kEvaluationCPUDuration, 0);

  // Although SubresourceFilterAgents still record the activation decision.
}

}  // namespace subresource_filter
