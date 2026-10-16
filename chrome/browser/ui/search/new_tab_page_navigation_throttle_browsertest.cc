// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

class NewTabPageNavigationThrottleTest : public InProcessBrowserTest {
 public:
  NewTabPageNavigationThrottleTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
  }

  void SetNewTabPage(const std::string& ntp_url) {
    // Set the new tab page.
    ntp_test_utils::SetUserSelectedDefaultSearchProvider(
        browser()->GetProfile(), https_test_server()->base_url().spec(),
        ntp_url);

    // Ensure we are using the newly set new_tab_url and won't be directed
    // to the local new tab page.
    TemplateURLService* service =
        TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
    search_test_utils::WaitForTemplateURLServiceToLoad(service);
    ASSERT_EQ(search::GetNewTabPageURL(browser()->GetProfile()), ntp_url);
  }

  // Navigates to the New Tab Page and then returns the GURL that ultimately was
  // navigated to.
  GURL NavigateToNewTabPage() {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), chrome::ChromeUINewTabURLAsGURL()));
    return web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  net::EmbeddedTestServer* https_test_server() { return &https_test_server_; }

  net::EmbeddedTestServer https_test_server_;
};

// Fixture that runs OverrideNavigationParams tests with the
// kNtpDisableBrowserInitiatedLinks feature explicitly enabled. The feature
// state must be set in the fixture constructor (before BrowserTestBase::SetUp)
// per base::test::ScopedFeatureList contract.
class NewTabPageNavigationThrottleNoBrowserInitiatedLinksTest
    : public NewTabPageNavigationThrottleTest {
 public:
  NewTabPageNavigationThrottleNoBrowserInitiatedLinksTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ntp_features::kNtpDisableBrowserInitiatedLinks);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Fixture that runs OverrideNavigationParams tests with the
// kNtpDisableBrowserInitiatedLinks feature explicitly disabled (legacy
// behavior).
class NewTabPageNavigationThrottleBrowserInitiatedLinksTest
    : public NewTabPageNavigationThrottleTest {
 public:
  NewTabPageNavigationThrottleBrowserInitiatedLinksTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ntp_features::kNtpDisableBrowserInitiatedLinks);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleTest, LocalNewTabPage) {
  ASSERT_TRUE(https_test_server()->Start());
  // This URL is not https so it will default to the 3P WebUI NTP.
  SetNewTabPage(chrome::kChromeUINewTabPageThirdPartyURL);
  // Already going to the 3P WebUI NTP, so we should arrive there as expected.
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, NavigateToNewTabPage());
}

// Snapshot of the NavigationHandle fields that OverrideNavigationParams may
// mutate. The NavigationHandle itself cannot be cloned, and the observer
// outlives DidFinishNavigation, so the test stashes the fields of interest
// and reads them back from the test body.
struct NavigationHandleSnapshot {
  ui::PageTransition page_transition = ui::PAGE_TRANSITION_LINK;
  bool is_renderer_initiated = false;
  std::optional<url::Origin> initiator_origin;
};

class OverrideNavigationParamsObserver : public content::WebContentsObserver {
 public:
  explicit OverrideNavigationParamsObserver(content::WebContents* contents)
      : WebContentsObserver(contents) {}

  // WebContentsObserver overrides:
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    EXPECT_TRUE(handle);
    if (!handle) {
      return;
    }
    last_snapshot_ = NavigationHandleSnapshot{
        handle->GetPageTransition(),
        handle->IsRendererInitiated(),
        handle->GetInitiatorOrigin(),
    };
  }

  const std::optional<NavigationHandleSnapshot>& last_snapshot() const {
    return last_snapshot_;
  }

 private:
  std::optional<NavigationHandleSnapshot> last_snapshot_;
};

// Check that ChromeContentBrowserClient::OverrideNavigationParams behaves
// correctly when navigating from a chrome:// NTP, with the
// kNtpDisableBrowserInitiatedLinks feature enabled.
IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleNoBrowserInitiatedLinksTest,
                       OverrideNavigationParams_ChromeURLNTP) {
  ASSERT_TRUE(https_test_server()->Start());
  SetNewTabPage(chrome::kChromeUINewTabPageThirdPartyURL);
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, NavigateToNewTabPage());

  const GURL page_url = https_test_server()->GetURL("/simple.html");
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  OverrideNavigationParamsObserver observer(web_contents);
  EXPECT_TRUE(content::NavigateToURLFromRenderer(web_contents, page_url));

  ASSERT_TRUE(observer.last_snapshot());
  const auto& snapshot = *observer.last_snapshot();
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(snapshot.page_transition,
                                           ui::PAGE_TRANSITION_AUTO_BOOKMARK));
  EXPECT_NE(std::nullopt, snapshot.initiator_origin);
  // The source is a chrome:// document, so renderer-side BeginNavigation
  // forks the navigation to OpenURL, which calls Navigator::RequestOpenURL
  // and flips is_renderer_initiated to false because of the WebUI check.
  // That happens independent of OverrideNavigationParams.
  EXPECT_FALSE(snapshot.is_renderer_initiated);
}

// Same as above but with the feature disabled.
IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleBrowserInitiatedLinksTest,
                       OverrideNavigationParams_ChromeURLNTP) {
  ASSERT_TRUE(https_test_server()->Start());
  SetNewTabPage(chrome::kChromeUINewTabPageThirdPartyURL);
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, NavigateToNewTabPage());

  const GURL page_url = https_test_server()->GetURL("/simple.html");
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  OverrideNavigationParamsObserver observer(web_contents);
  EXPECT_TRUE(content::NavigateToURLFromRenderer(web_contents, page_url));

  ASSERT_TRUE(observer.last_snapshot());
  const auto& snapshot = *observer.last_snapshot();
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(snapshot.page_transition,
                                           ui::PAGE_TRANSITION_AUTO_BOOKMARK));
  EXPECT_EQ(std::nullopt, snapshot.initiator_origin);
  EXPECT_FALSE(snapshot.is_renderer_initiated);
}

}  // namespace
