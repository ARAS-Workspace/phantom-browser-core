// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/chrome_prefetch_manager.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prefetch_test_util.h"
#include "content/public/test/preloading_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PrefetchBrowserTest : public PlatformBrowserTest {
 public:
  PrefetchBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features = {};

    feature_list_.InitWithFeatures(enabled_features, {});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    ssl_server_.ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(ssl_server_.Start());

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    attempt_entry_builder_ =
        std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
            PredictorToExpectInUkm());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    ASSERT_TRUE(ssl_server_.ShutdownAndWaitUntilComplete());
  }

  GURL GetURL(const std::string& path) {
    return ssl_server_.GetURL("a.test", path);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  bool NavigateToURL(const GURL& url) {
    return content::NavigateToURL(GetActiveWebContents(), url);
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const content::test::PreloadingAttemptUkmEntryBuilder&
  attempt_entry_builder() {
    return *attempt_entry_builder_;
  }

 protected:
  // Used when creating `test_ukm_recorder_`.
  content::PreloadingPredictor PredictorToExpectInUkm() {
    return chrome_preloading_predictor::kChromeCustomTabs;
  }

 private:
  base::ScopedMockElapsedTimersForTest test_timer_;
  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  ::test::ScopedPrewarmFeatureList prewarm_feature_list_{
      ::test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      attempt_entry_builder_;
};

}  // namespace
