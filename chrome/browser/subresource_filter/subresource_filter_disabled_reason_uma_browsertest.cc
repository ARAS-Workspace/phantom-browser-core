// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

const char kDisabledReasonHistogram[] =
    "SubresourceFilter.PageLoad.ActivationState.DisabledReason";

}  // namespace

class SubresourceFilterDisabledReasonUmaBrowserTest
    : public SubresourceFilterBrowserTest {
 public:
  SubresourceFilterDisabledReasonUmaBrowserTest() {
    // Disable `kPrewarm` to prevent any pre-warm navigation from firing during
    // test setup, which can race with the main navigation initiated by the test
    // body. This test suite relies on HistogramTester to assert UMA counts for
    // a single page load, so an unexpected pre-warm navigation would
    // contaminate the histogram data and cause test failures.
    prewarm_feature_.InitAndDisableFeature(features::kPrewarm);
  }

  SubresourceFilterDisabledReasonUmaBrowserTest(
      const SubresourceFilterDisabledReasonUmaBrowserTest&) = delete;
  SubresourceFilterDisabledReasonUmaBrowserTest& operator=(
      const SubresourceFilterDisabledReasonUmaBrowserTest&) = delete;

  ~SubresourceFilterDisabledReasonUmaBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  GURL GetURL(const std::string& page) {
    return embedded_test_server()->GetURL("foo.com", "/" + page);
  }

 private:
  base::test::ScopedFeatureList prewarm_feature_;
};


IN_PROC_BROWSER_TEST_F(SubresourceFilterDisabledReasonUmaBrowserTest,
                       NoMatchingConfiguration_UrlNotOnPhishingList) {
  base::HistogramTester histogram_tester;
  GURL url = GetURL("subresource_filter/frame_with_no_subresources.html");

  Configuration config(mojom::ActivationLevel::kEnabled,
                       ActivationScope::ACTIVATION_LIST,
                       ActivationList::PHISHING_INTERSTITIAL);
  ResetConfiguration(std::move(config));

  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  // Navigate to a URL not on the phishing list.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  histogram_tester.ExpectUniqueSample(
      kDisabledReasonHistogram,
      mojom::SubresourceFilterDisabledReason::kNoMatchingConfiguration, 1);
}



IN_PROC_BROWSER_TEST_F(SubresourceFilterDisabledReasonUmaBrowserTest,
                       DisabledByConfiguration) {
  base::HistogramTester histogram_tester;
  GURL url = GetURL("subresource_filter/frame_with_no_subresources.html");

  Configuration config(mojom::ActivationLevel::kDisabled,
                       ActivationScope::ALL_SITES);
  ResetConfiguration(std::move(config));

  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  histogram_tester.ExpectUniqueSample(
      kDisabledReasonHistogram,
      mojom::SubresourceFilterDisabledReason::kDisabledByConfiguration, 1);
}





class SubresourceFilterDisabledReasonAdTaggingDisabledBrowserTest
    : public SubresourceFilterDisabledReasonUmaBrowserTest {
 public:
  SubresourceFilterDisabledReasonAdTaggingDisabledBrowserTest() {
    // Disable ad tagging to prevent a default dry-run activation.
    feature_list_.InitAndDisableFeature(kAdTagging);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SubresourceFilterDisabledReasonAdTaggingDisabledBrowserTest,
    FilterNeverCreated) {
  base::HistogramTester histogram_tester;
  GURL url = GetURL("subresource_filter/frame_with_no_subresources.html");

  // Null out the database manager. With no database manager, the activation
  // computing throttle is created but never told to activate, so it never
  // creates an AsyncDocumentSubresourceFilter. This is the scenario for
  // kFilterNeverCreated.
  auto* helper = subresource_filter::ContentSubresourceFilterWebContentsHelper::
      FromWebContents(browser()->tab_strip_model()->GetActiveWebContents());
  helper->SetDatabaseManagerForTesting(nullptr);

  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  histogram_tester.ExpectUniqueSample(
      kDisabledReasonHistogram,
      mojom::SubresourceFilterDisabledReason::kFilterNeverCreated, 1);
}

}  // namespace subresource_filter
