// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ThirdPartyNTPBrowserTest : public InProcessBrowserTest,
                                 public InstantTestBase,
                                 public ::testing::WithParamInterface<bool> {
 public:
  ThirdPartyNTPBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kInstantUsesSpareRenderer, InstantUsesSpareRenderer());
  }

  ThirdPartyNTPBrowserTest(const ThirdPartyNTPBrowserTest&) = delete;
  ThirdPartyNTPBrowserTest& operator=(const ThirdPartyNTPBrowserTest&) = delete;

  bool InstantUsesSpareRenderer() const { return GetParam(); }

  void SetUpOnMainThread() override {
    https_test_server().RegisterRequestHandler(base::BindRepeating(
        &ThirdPartyNTPBrowserTest::HandleRedirectToChromeSearch,
        base::Unretained(this)));
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server().SetCertHostnames({"example.com", "ntp.example.com"});
    ASSERT_TRUE(https_test_server().Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRedirectToChromeSearch(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/redirect-to-chrome-search") {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_FOUND);
    response->AddCustomHeader("Location",
                              "chrome-search://theme/current-channel-logo");
    response->set_content_type("text/html");
    response->set_content("<p>Redirecting to chrome-search</p>");
    return response;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(InstantUsesSpareRenderer,
                         ThirdPartyNTPBrowserTest,
                         testing::Bool());

// Verifies that a non-instant page cannot embed an iframe with the
// chrome-search scheme.
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest,
                       NonInstantProcessCannotEmbedChromeSearch) {
  GURL non_instant_url = https_test_server().GetURL("example.com", "/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_instant_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Add a chrome-search://theme/current-channel-logo subframe and
  // verify that the resource is not allowed to load.
  const char kChromeSearchIframeUrl[] =
      "chrome-search://theme/current-channel-logo";

  // The test script string is the same for both branches, so define it once.
  std::string script =
      base::StrCat({"const frame = document.createElement('iframe');"
                    "frame.src = '",
                    kChromeSearchIframeUrl,
                    "';"
                    "document.body.appendChild(frame);"});

  if (InstantUsesSpareRenderer()) {
    // When feature is enabled: browser-side blocking via NavigationThrottle.
    // Navigation is blocked and should not load the chrome-search URL.
    content::TestNavigationObserver nav_observer(contents);
    ASSERT_TRUE(content::ExecJs(contents, script));
    nav_observer.WaitForNavigationFinished();

    content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
    ASSERT_TRUE(subframe);
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
    EXPECT_TRUE(subframe->IsErrorDocument());
  } else {
    // When feature is disabled: renderer-side blocking, console error fires
    content::WebContentsConsoleObserver console_observer(contents);
    console_observer.SetPattern(
        std::string("Not allowed to load local resource: ") +
        kChromeSearchIframeUrl);
    ASSERT_TRUE(content::ExecJs(contents, script));
    ASSERT_TRUE(console_observer.Wait());

    // Verify that the subframe does not load the chrome-search url.
    content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
    ASSERT_TRUE(subframe);
    EXPECT_EQ(GURL(), subframe->GetLastCommittedURL());
  }
}

// Verifies that a non-instant page cannot navigate to a chrome-search scheme
// via renderer-initiated navigation.
IN_PROC_BROWSER_TEST_P(
    ThirdPartyNTPBrowserTest,
    NonInstantProcessBlocksRendererInitiatedChromeSearchNavigation) {
  GURL non_instant_url = https_test_server().GetURL("example.com", "/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_instant_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Start a renderer initiated navigation to the chrome-search://
  // url and verify that the resource is not allowed to load.
  const char kChromeSearchUrl[] = "chrome-search://theme/current-channel-logo";

  if (InstantUsesSpareRenderer()) {
    // When feature is enabled: browser-side blocking via NavigationThrottle.
    // Navigation is blocked and should not load the chrome-search URL. For
    // this main-frame navigation we expect an error page to be shown.
    content::TestNavigationObserver nav_observer(contents);
    std::string script =
        base::StrCat({"window.location= '", kChromeSearchUrl, "';"});
    ASSERT_TRUE(content::ExecJs(contents, script));
    nav_observer.WaitForNavigationFinished();

    // Navigation should fail (be blocked).
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
    EXPECT_TRUE(contents->GetPrimaryMainFrame()->IsErrorDocument());
  } else {
    // When feature is disabled: renderer-side blocking, console error fires
    content::WebContentsConsoleObserver console_observer(contents);
    console_observer.SetPattern(
        std::string("Not allowed to load local resource: ") + kChromeSearchUrl);
    std::string script =
        base::StrCat({"window.location= '", kChromeSearchUrl, "';"});
    ASSERT_TRUE(content::ExecJs(contents, script));
    ASSERT_TRUE(console_observer.Wait());
    // Verify that the main frame does not navigate to the chrome-search url.
    EXPECT_EQ(non_instant_url,
              contents->GetPrimaryMainFrame()->GetLastCommittedURL());
  }
}

// Verifies that renderer-initiated navigation to a page whose response
// redirects to a chrome-search scheme cannot commit.
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest,
                       ResponseRedirectToChromeSearchBlockedAtCommit) {
  GURL non_instant_url = https_test_server().GetURL("example.com", "/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_instant_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL redirect_url =
      https_test_server().GetURL("example.com", "/redirect-to-chrome-search");

  ASSERT_TRUE(content::ExecJs(
      contents,
      base::StrCat({"window.location = '", redirect_url.spec(), "';"})));
  content::WaitForLoadStopWithoutSuccessCheck(contents);
  EXPECT_EQ(non_instant_url,
            contents->GetPrimaryMainFrame()->GetLastCommittedURL());
}

// Verifies that a non-instant process cannot fetch chrome-search resources.
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest,
                       NonInstantProcessCannotFetchChromeSearchResource) {
  GURL non_instant_url = https_test_server().GetURL("example.com", "/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_instant_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Verify that the process is not marked as an Instant process.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->GetProfile());
  EXPECT_FALSE(instant_service->IsInstantProcess(
      contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID()));

  // Try to fetch a chrome-search resource using XMLHttpRequest and verify it
  // fails.
  const char* kScript = R"(
    (async () => {
      return new Promise((resolve) => {
        const xhr = new XMLHttpRequest();
        xhr.open('GET', 'chrome-search://theme/current-channel-logo');
        xhr.onload = () => resolve('success');
        xhr.onerror = () => resolve('error: Failed to fetch');
        xhr.send();
      });
    })()
  )";
  std::string result = content::EvalJs(contents, kScript).ExtractString();
  EXPECT_EQ("error: Failed to fetch", result)
      << "Non-instant process should not be able to fetch chrome-search "
         "resources, but got: "
      << result;
}
