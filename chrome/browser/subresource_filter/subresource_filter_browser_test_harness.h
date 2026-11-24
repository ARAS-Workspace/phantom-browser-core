// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace proto = url_pattern_index::proto;

using subresource_filter::testing::TestRulesetPublisher;
using subresource_filter::testing::TestRulesetCreator;
using subresource_filter::testing::TestRulesetPair;

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content


namespace subresource_filter {

class RulesetService;

// Class for shared test harness functionality to be used for both safe browsing
// and fingerprinting protection browser tests.
class SubresourceFilterSharedBrowserTest : public PlatformBrowserTest {
 public:
  SubresourceFilterSharedBrowserTest();

  SubresourceFilterSharedBrowserTest(
      const SubresourceFilterSharedBrowserTest&) = delete;
  SubresourceFilterSharedBrowserTest& operator=(
      const SubresourceFilterSharedBrowserTest&) = delete;

  ~SubresourceFilterSharedBrowserTest() override;

 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override;

  virtual GURL GetTestUrl(const std::string& relative_url) const;

  content::WebContents* web_contents();

  bool WasParsedScriptElementLoaded(content::RenderFrameHost* rfh);
};

class SubresourceFilterBrowserTest : public SubresourceFilterSharedBrowserTest {
 public:
  SubresourceFilterBrowserTest();

  SubresourceFilterBrowserTest(const SubresourceFilterBrowserTest&) = delete;
  SubresourceFilterBrowserTest& operator=(const SubresourceFilterBrowserTest&) =
      delete;

  ~SubresourceFilterBrowserTest() override;

  // Names of DocumentLoad histograms.

  static constexpr const char kSubresourceLoadsTotalForPage[] =
      "SubresourceFilter.PageLoad.NumSubresourceLoads.Total";
  static constexpr const char kSubresourceLoadsEvaluatedForPage[] =
      "SubresourceFilter.PageLoad.NumSubresourceLoads.Evaluated";
  static constexpr const char kSubresourceLoadsMatchedRulesForPage[] =
      "SubresourceFilter.PageLoad.NumSubresourceLoads.MatchedRules";
  static constexpr const char kSubresourceLoadsDisallowedForPage[] =
      "SubresourceFilter.PageLoad.NumSubresourceLoads.Disallowed";

  // Names of the performance measurement histograms.
  static constexpr const char kEvaluationTotalWallDurationForPage[] =
      "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalWallDuration";
  static constexpr const char kEvaluationTotalCPUDurationForPage[] =
      "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalCPUDuration";
  static constexpr const char kEvaluationWallDuration[] =
      "SubresourceFilter.SubresourceLoad.Evaluation.WallDuration";
  static constexpr const char kEvaluationCPUDuration[] =
      "SubresourceFilter.SubresourceLoad.Evaluation.CPUDuration";

 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override;

  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix);

  void SetRulesetWithRules(const std::vector<proto::UrlRule>& rules);

  // Re-initializes the ruleset_service by opening the ruleset file provided
  // by indexed_ruleset_path and publishing it.
  void OpenAndPublishRuleset(RulesetService* ruleset_service,
                             const base::FilePath& indexed_ruleset_path);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  TestRulesetCreator ruleset_creator_;
};

}  // namespace subresource_filter

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_
