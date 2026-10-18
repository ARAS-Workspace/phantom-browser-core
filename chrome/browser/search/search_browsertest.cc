// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include <stddef.h>

#include <array>
#include <map>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"

namespace search {

bool MatchesOriginAndPath(const GURL& my_url, const GURL& other_url);

TEST(SearchURLsTest, MatchesOriginAndPath) {
  EXPECT_TRUE(MatchesOriginAndPath(GURL("http://example.com/path"),
                                   GURL("http://example.com/path?param")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("http://not.example.com/path"),
                                    GURL("http://example.com/path")));
  EXPECT_TRUE(MatchesOriginAndPath(GURL("http://example.com:80/path"),
                                   GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("http://example.com:8080/path"),
                                    GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("ftp://example.com/path"),
                                    GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("http://example.com/path"),
                                    GURL("https://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("https://example.com/path"),
                                    GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("http://example.com/path"),
                                    GURL("http://example.com/another-path")));
}

class SearchTest : public MixinBasedInProcessBrowserTest {
 public:
  Profile* profile() const { return browser()->GetProfile(); }

 protected:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url.host() == "foo.com") {
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 200 OK\nContent-Type: text/html\n",
                    "<html><body>OK</body></html>", params->client.get());
                return true;
              }
              if (params->url_request.url.host() == "insecure.com") {
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 403 Forbidden\nContent-Type: text/html\n",
                    "<html><body>Forbidden</body></html>",
                    params->client.get());
                return true;
              }
              return false;
            }));

    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
    SetSearchProvider(true, false);
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  virtual void SetSearchProvider(bool set_ntp_url, bool insecure_ntp_url) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    TemplateURLData data;
    data.SetShortName(u"foo.com");
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    if (set_ntp_url) {
      data.new_tab_url = (insecure_ntp_url ? "http" : "https") +
                         std::string("://foo.com/newtab");
    }
    data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");

    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

struct SearchTestCase {
  const char* url;
  bool expected_result;
  const char* comment;
};

IN_PROC_BROWSER_TEST_F(SearchTest,
                       InstantCacheableNTPNavigationEntryNewProfile) {
  SetSearchProvider(false, false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           chrome::ChromeUINewTabURLAsGURL()));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = contents->GetController();
  // Test virtual url chrome://newtab for first NTP of a new profile
  EXPECT_TRUE(
      MatchesOriginAndPath(GURL(chrome::kChromeUINewTabPageThirdPartyURL),
                           controller.GetLastCommittedEntry()->GetURL()));
  // The new_tab_url gets set after the first NTP is visible.
  SetSearchProvider(true, false);
  EXPECT_TRUE(
      MatchesOriginAndPath(GURL(chrome::kChromeUINewTabPageThirdPartyURL),
                           controller.GetLastCommittedEntry()->GetURL()));
}

IN_PROC_BROWSER_TEST_F(SearchTest, NoRewriteInIncognito) {
  Profile* incognito =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(GURL(), GetNewTabPageURL(incognito));
  GURL new_tab_url = chrome::ChromeUINewTabURLAsGURL();
  EXPECT_FALSE(HandleNewTabURLRewrite(&new_tab_url, incognito));
  EXPECT_EQ(chrome::ChromeUINewTabURLAsGURL(), new_tab_url);
}

IN_PROC_BROWSER_TEST_F(SearchTest, UseLocalNTPIfNTPURLIsInsecure) {
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(true, true);
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL,
            GetNewTabPageURL(profile()));
  GURL new_tab_url = chrome::ChromeUINewTabURLAsGURL();
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, new_tab_url);
}

IN_PROC_BROWSER_TEST_F(SearchTest, UseLocalNTPIfNTPURLIsNotSet) {
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(false, true);
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL,
            GetNewTabPageURL(profile()));
  GURL new_tab_url = chrome::ChromeUINewTabURLAsGURL();
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, new_tab_url);
}

IN_PROC_BROWSER_TEST_F(SearchTest, IsNTPOrRelatedURL) {
  GURL invalid_url;
  const GURL& ntp_url = chrome::ChromeUINewTabURLAsGURL();

  EXPECT_FALSE(IsNTPOrRelatedURL(invalid_url, profile()));

  GURL new_tab_page_url(GetNewTabPageURL(profile()));
  GURL search_url_with_search_terms("https://foo.com/url?bar=abc");
  GURL search_url_without_search_terms("https://foo.com/url?bar");

  EXPECT_FALSE(IsNTPOrRelatedURL(ntp_url, profile()));
  EXPECT_TRUE(IsNTPOrRelatedURL(new_tab_page_url, profile()));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_with_search_terms, profile()));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_without_search_terms, profile()));

  EXPECT_FALSE(IsNTPOrRelatedURL(ntp_url, nullptr));
  EXPECT_FALSE(IsNTPOrRelatedURL(new_tab_page_url, nullptr));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_with_search_terms, nullptr));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_without_search_terms, nullptr));
}

// Tests whether a |url| corresponds to a New Tab page.
// See search::IsNTPURL(const GURL& url);
IN_PROC_BROWSER_TEST_F(SearchTest, IsNTPURL) {
  const auto kTestCases = std::to_array<SearchTestCase>({
      {"chrome-search://remote-ntp", false, "Remote NTP URL"},
      {"chrome://new-tab-page-third-party", true, "WebUI NTP"},
      {"chrome://new-tab-page-third-party/path?params", true,
       "WebUI NTP with path and params"},
      {"invalid-scheme://remote-ntp", false, "Invalid Remote NTP URL"},
      {"chrome-search://most-visited/", false, "Most visited URL"},
      {"", false, "Invalid URL"},
  });

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result, IsNTPURL(GURL(test.url)))
        << test.url << " " << test.comment;
  }
}

}  // namespace search
