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
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/navigation_handle.h"
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
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "url/gurl.h"

namespace subresource_filter {

using subresource_filter::testing::TestRulesetPair;

namespace {

namespace proto = url_pattern_index::proto;

// The path to a multi-frame document used for tests.
static constexpr const char kTestFrameSetPath[] =
    "/subresource_filter/frame_set.html";

}  // namespace

// Tests -----------------------------------------------------------------------
















IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PRE_MainFrameActivationOnStartup) {
  SetRulesetToDisallowURLsWithPathSuffix("included_script.js");
}





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

// Test that resources in frames with an aborted initial load due to a doc.write
// are still disallowed.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       FrameWithDocWriteAbortedLoad_ResourceStillDisallowed) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  // Watches for title set by onload and onerror callbacks of tested resource
  content::TitleWatcher title_watcher(web_contents(), u"failed");
  title_watcher.AlsoWaitForTitle(u"loaded");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/subresource_filter/docwrite_loads_disallowed_resource.html")));

  // Check the load was blocked.
  EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());
}

// Test that resources in frames with an aborted initial load due to a
// window.stop are still disallowed.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       FrameWithWindowStopAbortedLoad_ResourceStillDisallowed) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  // Watches for title set by onload and onerror callbacks of tested resource
  content::TitleWatcher title_watcher(web_contents(), u"failed");
  title_watcher.AlsoWaitForTitle(u"loaded");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/subresource_filter/window_stop_loads_disallowed_resource.html")));

  // Check the load was blocked.
  EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());
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

// Test that an allowed resource in the child of a frame with its initial load
// aborted due to a doc.write is not blocked.
IN_PROC_BROWSER_TEST_F(
    SubresourceFilterBrowserTest,
    ChildOfFrameWithAbortedLoadLoadsAllowedResource_ResourceLoaded) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  // Watches for title set by onload and onerror callbacks of tested resource.
  content::TitleWatcher title_watcher(web_contents(), u"failed");
  title_watcher.AlsoWaitForTitle(u"loaded");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/subresource_filter/"
                                     "docwrite_creates_subframe.html")));

  content::RenderFrameHost* frame = FindFrameByName("grandchild");

  EXPECT_TRUE(ExecJs(frame, R"SCRIPT(
      let image = document.createElement('img');
      image.src = 'pixel.png';
      image.onload = function() {
        top.document.title='loaded';
      };
      image.onerror = function() {
        top.document.title='failed';
      };
      document.body.appendChild(image);
  )SCRIPT"));

  // Check the load wasn't blocked.
  EXPECT_EQ(u"loaded", title_watcher.WaitAndGetTitle());
}

// Test that a disallowed resource in the child of a frame with its initial load
// aborted due to a doc.write is blocked.
IN_PROC_BROWSER_TEST_F(
    SubresourceFilterBrowserTest,
    ChildOfFrameWithAbortedLoadLoadsDisallowedResource_ResourceBlocked) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  // Watches for title set by onload and onerror callbacks of tested resource.
  content::TitleWatcher title_watcher(web_contents(), u"failed");
  title_watcher.AlsoWaitForTitle(u"loaded");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/subresource_filter/"
                                     "docwrite_creates_subframe.html")));

  content::RenderFrameHost* frame = FindFrameByName("grandchild");

  EXPECT_TRUE(ExecJs(frame, R"SCRIPT(
      let image = document.createElement('img');
      image.src = 'pixel.png?ad=true';
      image.onload = function() {
        top.document.title='loaded';
      };
      image.onerror = function() {
        top.document.title='failed';
      };
      document.body.appendChild(image);
  )SCRIPT"));

  // Check the load was blocked.
  EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PopupsInheritActivation_ResourcesBlocked) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  const std::vector<std::string> test_case_scripts = {
      // Popup to URL
      "window.open('/subresource_filter/popup.html');",

      // Popup to empty URL
      "popupLoadsDisallowedResource('');",

      // Child of popup to empty URL
      "popupLoadsDisallowedResourceAsDescendant('');",

      // Popup to about:blank URL. about:blank popups behave differently to
      // popups with an empty URL, so we test them separately.
      "popupLoadsDisallowedResource('about:blank');",

      // Child of popup to about:blank URL
      "popupLoadsDisallowedResourceAsDescendant('about:blank');",

      // Popup with doc.write-aborted load
      "popupLoadsDisallowedResource('http://b.com/slow?100');",

      // TODO(alexmt): Enable this test case. Currently disabled as there is no
      // guarantee that the descendant's navigation starts after the parent's
      // navigation ends (see crbug.com/40138406).
      // Child of popup with doc.write-aborted load
      // "popupLoadsDisallowedResourceAsDescendant('http://b.com/slow?100');",

  };

  for (const auto& test_case_script : test_case_scripts) {
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "/subresource_filter/popup_disallowed_load_helper.html")));
    ASSERT_TRUE(ExecJs(web_contents(), test_case_script));
    content::TitleWatcher title_watcher(popup_observer.GetWebContents(),
                                        u"failed");
    title_watcher.AlsoWaitForTitle(u"loaded");

    // Check the load was blocked.
    EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());
  }
}


// Test that resources in a popup with an aborted initial load due to a
// doc.write are still blocked when disallowed, even if the opener is
// immediately closed after writing.
// TODO(alexmt): Fix test flakiness and then reenable.
IN_PROC_BROWSER_TEST_F(
    SubresourceFilterBrowserTest,
    DISABLED_PopupWithDocWriteAbortedLoadAndOpenerClosed_FilterChecked) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("ad_script.js"),
                           testing::CreateSuffixRule("ad=true")}));

  // Block disallowed resources.
  Configuration config(subresource_filter::mojom::ActivationLevel::kEnabled,
                       subresource_filter::ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  content::WebContents* original_web_contents = web_contents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  base::HistogramTester tester;
  content::WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(ExecJs(original_web_contents, R"SCRIPT(
    popup = window.open('http://b.com/slow?100');
    window.onunload = function(e){
      doc = popup.document;
      doc.open();
      doc.write(
        "<html><body>Rewritten. <img src='/ad_tagging/pixel.png?ad=true' " +
        "onload='window.document.title = \"loaded\";' " +
        "onerror='window.document.title = \"failed\";'></body></html>");
      doc.close();
    };
    )SCRIPT"));
  original_web_contents->ClosePage();

  content::TitleWatcher title_watcher(popup_observer.GetWebContents(),
                                      u"failed");
  title_watcher.AlsoWaitForTitle(u"loaded");

  // Check the load was blocked.
  EXPECT_EQ(u"failed", title_watcher.WaitAndGetTitle());

  // Check histograms agree that activation was inherited.
  tester.ExpectBucketCount(kPageLoadActivationStateHistogram,
                           static_cast<int>(mojom::ActivationLevel::kEnabled),
                           1);
  tester.ExpectBucketCount(kPageLoadActivationStateDidInheritHistogram,
                           static_cast<int>(mojom::ActivationLevel::kEnabled),
                           1);
}

// Tests checking how histograms are recorded. ---------------------------------

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40236757): Flaky on Mac.
#define MAYBE_ExpectPerformanceHistogramsAreRecorded \
  DISABLED_ExpectPerformanceHistogramsAreRecorded
#else
#define MAYBE_ExpectPerformanceHistogramsAreRecorded \
  ExpectPerformanceHistogramsAreRecorded
#endif

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
