// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/webshare/mac/sharing_service_operation.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#endif

class ShareServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(IS_MAC)
    webshare::SharingServiceOperation::SetSharePickerCallbackForTesting(
        base::BindRepeating(&ShareServiceBrowserTest::AcceptShareRequest));
#endif
  }

#if BUILDFLAG(IS_MAC)
  static void AcceptShareRequest(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::string& text,
      const std::string& title,
      const GURL& url,
      blink::mojom::ShareService::ShareCallback close_callback) {
    std::move(close_callback).Run(blink::mojom::ShareError::OK);
  }
#endif

 private:
};

IN_PROC_BROWSER_TEST_F(ShareServiceBrowserTest, Text) {
  const int kRepeats = 4;

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/webshare/index.html")));

  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const std::string script = "share_text('hello')";
  for (int index = 0; index < kRepeats; ++index) {
    const content::EvalJsResult result = content::EvalJs(contents, script);
    EXPECT_EQ("share succeeded", result);
  }

  histogram_tester.ExpectBucketCount(kWebShareApiCountMetric,
                                     WebShareMethod::kShare, kRepeats);
}

IN_PROC_BROWSER_TEST_F(ShareServiceBrowserTest, InactiveWebContents) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/webshare/index.html")));
  content::WebContents* contents_0 =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a split and verify there are now 2 tabs
  chrome::NewSplitTab(browser(), split_tabs::SplitTabLayout::kSideBySide,
                      split_tabs::SplitTabCreatedSource::kToolbarButton);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(browser()->tab_strip_model()->GetTabAtIndex(0)->IsSplit());
  EXPECT_TRUE(browser()->tab_strip_model()->GetTabAtIndex(1)->IsSplit());

  // Tab 0 is now inactive.
  tabs::TabInterface* tab_0 = tabs::TabInterface::GetFromContents(contents_0);
  EXPECT_FALSE(tab_0->IsActivated());

  // Initiate share from tab 0. Permission in denied because it's inactive.
  std::string result =
      content::EvalJs(contents_0, "share_text('hello')").ExtractString();
  EXPECT_THAT(result, testing::HasSubstr("share failed"));
  EXPECT_THAT(result, testing::HasSubstr("NotAllowedError"));
}

class ShareServicePrerenderBrowserTest : public ShareServiceBrowserTest {
 public:
  ShareServicePrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&ShareServicePrerenderBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~ShareServicePrerenderBrowserTest() override = default;

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(ShareServicePrerenderBrowserTest, Text) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Start a prerender.
  const GURL kPrerenderUrl =
      embedded_test_server()->GetURL("/webshare/index.html");
  const content::PrerenderHostId kPrerenderHostId =
      prerender_helper_.AddPrerender((kPrerenderUrl));
  ASSERT_EQ(prerender_helper_.GetHostForUrl(kPrerenderUrl), kPrerenderHostId);

  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(kPrerenderHostId);
  EXPECT_EQ(prerender_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);
  const std::string script = "share_text('hello')";
  const content::EvalJsResult prerendered_result =
      content::EvalJs(prerender_rfh, script);
  EXPECT_EQ(
      "share failed: NotAllowedError: Failed to execute 'share' on "
      "'Navigator': Must be handling a user gesture to perform a share "
      "request.",
      prerendered_result);
  histogram_tester.ExpectBucketCount(kWebShareApiCountMetric,
                                     WebShareMethod::kShare, 0);

  // Activate the prerendered page.
  prerender_helper_.NavigatePrimaryPage(kPrerenderUrl);
  EXPECT_EQ(prerender_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);
  ASSERT_EQ(kPrerenderUrl, contents->GetLastCommittedURL());
  const content::EvalJsResult activated_result =
      content::EvalJs(prerender_rfh, script);
  EXPECT_EQ("share succeeded", activated_result);
  histogram_tester.ExpectBucketCount(kWebShareApiCountMetric,
                                     WebShareMethod::kShare, 1);
}
