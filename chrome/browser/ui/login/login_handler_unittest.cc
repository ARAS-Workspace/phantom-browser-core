// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/login/login_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/base/auth.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace {

const char kHttpUrl[] = "http://example.com/foo/bar";
const char kBasicAuthScheme[] = "Basic";
const char kFooRealm[] = "Foo";
const char kInsecureProxy[] = "Your connection to this site is not private";

enum TargetType { PROXY, SERVER };

const struct TestCase {
  const char* const request_url;
  struct {
    TargetType target_type;
    const char* const scheme;
    const char* const realm;
    const char* const challenger;
  } auth_info;
  struct {
    const char* const authority;
    const char* const explanation;
  } expected;
} kTestCases[]{
    // Insecure proxy
    {kHttpUrl,
     {PROXY, kBasicAuthScheme, kFooRealm, "http://example.com"},
     {"The proxy http://example.com", kInsecureProxy}},

    // Insecure proxy on non-standard port
    {kHttpUrl,
     {PROXY, kBasicAuthScheme, kFooRealm, "http://example.com:8009"},
     {"The proxy http://example.com:8009", kInsecureProxy}},

    // Secure proxy
    {kHttpUrl,
     {PROXY, kBasicAuthScheme, kFooRealm, "https://example.com"},
     {"The proxy https://example.com", ""}},

    // Secure proxy on non-standard port
    {kHttpUrl,
     {PROXY, kBasicAuthScheme, kFooRealm, "https://example.com:446"},
     {"The proxy https://example.com:446", ""}},

    // localhost
    {kHttpUrl,
     {PROXY, kBasicAuthScheme, kFooRealm, "http://localhost:7323"},
     {"The proxy http://localhost:7323", ""}},

    // Secure server
    {"https://www.nowhere.org/dir/index.html",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org", ""}},

    // URL uses default port.
    {"https://www.nowhere.org:443/dir/index.html",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org", ""}},

    // URL uses non-default port.
    {"https://www.nowhere.org:8443/dir/index.html",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org:8443", ""}},

    // URL has no trailing slash.
    {"https://www.nowhere.org",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org", ""}},

    // username:password
    {"https://foo:bar@www.nowhere.org/dir/index.html",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org", ""}},

    // query
    {"https://www.nowhere.org/dir/index.html?id=965362",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org", ""}},

    // reference
    {"https://www.nowhere.org/dir/index.html#toc",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org", ""}},
};

std::u16string ExpectedAuthority(bool is_proxy, const char* prefix) {
  std::u16string str = base::ASCIIToUTF16(prefix);
  // Proxies and Android have additional surrounding text. Otherwise, only the
  // host URL is shown.
  bool extra_text = is_proxy;
  if (extra_text) {
    str += u" requires a username and password.";
  }

  return str;
}

class LoginHandlerWithWebContentsTest : public ChromeRenderViewHostTestHarness {
 public:
  LoginHandlerWithWebContentsTest() = default;

  LoginHandlerWithWebContentsTest(const LoginHandlerWithWebContentsTest&) =
      delete;
  LoginHandlerWithWebContentsTest& operator=(
      const LoginHandlerWithWebContentsTest&) = delete;

  ~LoginHandlerWithWebContentsTest() override = default;
};

}  // namespace

TEST(LoginHandlerTest, DialogStringsAndRealm) {
  for (const auto& test_case : kTestCases) {
    net::AuthChallengeInfo auth_info;
    GURL request_url(test_case.request_url);
    auth_info.is_proxy = test_case.auth_info.target_type == PROXY;
    auth_info.scheme = test_case.auth_info.scheme;
    auth_info.realm = test_case.auth_info.realm;
    auth_info.challenger = url::SchemeHostPort(
        test_case.auth_info.challenger ? GURL(test_case.auth_info.challenger)
                                       : request_url);

    SCOPED_TRACE(::testing::Message()
                 << "request_url:" << test_case.request_url
                 << " auth_info: { is_proxy:" << auth_info.is_proxy
                 << " scheme:'" << auth_info.scheme << "' realm:'"
                 << auth_info.realm << "' challenger:'"
                 << auth_info.challenger.Serialize() << "' }");
    std::u16string authority;
    std::u16string explanation;

    LoginHandler::GetDialogStrings(request_url, auth_info, &authority,
                                   &explanation);
    EXPECT_EQ(ExpectedAuthority(test_case.auth_info.target_type == PROXY,
                                test_case.expected.authority),
              authority);
    EXPECT_STREQ(test_case.expected.explanation,
                 base::UTF16ToASCII(explanation).c_str());
  }
}

// Tests that LoginTabHelper does not crash if
// WillProcessMainFrameUnauthorizedResponse() is called when there is no pending
// entry. Regression test for https://crbug.com/40653919.
TEST_F(LoginHandlerWithWebContentsTest, NoPendingEntryDoesNotCrash) {
  LoginTabHelper::CreateForWebContents(web_contents());
  LoginTabHelper* helper = LoginTabHelper::FromWebContents(web_contents());
  net::AuthChallengeInfo challenge;
  content::MockNavigationHandle handle;
  handle.SetAuthChallengeInfo(challenge);
  handle.set_global_request_id({network::OriginatingProcessId::browser(), 1});
  content::NavigationThrottle::ThrottleCheckResult result =
      helper->WillProcessMainFrameUnauthorizedResponse(&handle);
  EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
}

// Tests that WillProcessMainFrameUnauthorizedResponse() cancels the navigation
// to show a login prompt when the auth challenge's challenger matches the
// navigation URL.
TEST_F(LoginHandlerWithWebContentsTest, MatchingChallengerCancelsForPrompt) {
  LoginTabHelper::CreateForWebContents(web_contents());
  LoginTabHelper* helper = LoginTabHelper::FromWebContents(web_contents());

  net::AuthChallengeInfo challenge;
  challenge.is_proxy = false;
  challenge.challenger = url::SchemeHostPort(GURL("https://requested.test"));

  content::MockNavigationHandle handle;
  handle.set_url(GURL("https://requested.test/page"));
  handle.SetAuthChallengeInfo(challenge);
  handle.set_global_request_id({network::OriginatingProcessId::browser(), 1});

  content::NavigationThrottle::ThrottleCheckResult result =
      helper->WillProcessMainFrameUnauthorizedResponse(&handle);
  EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
}

// Tests that WillProcessMainFrameUnauthorizedResponse() does not cancel the
// navigation to show a login prompt when the auth challenge's challenger does
// not match the navigation URL. This can occur when a service worker responds
// to a navigation with a 401 response that it fetched from a different origin.
TEST_F(LoginHandlerWithWebContentsTest, MismatchedChallengerDoesNotShowPrompt) {
  LoginTabHelper::CreateForWebContents(web_contents());
  LoginTabHelper* helper = LoginTabHelper::FromWebContents(web_contents());

  net::AuthChallengeInfo challenge;
  challenge.is_proxy = false;
  challenge.challenger = url::SchemeHostPort(GURL("https://other.test"));

  content::MockNavigationHandle handle;
  handle.set_url(GURL("https://requested.test/page"));
  handle.SetAuthChallengeInfo(challenge);
  handle.set_global_request_id({network::OriginatingProcessId::browser(), 1});

  content::NavigationThrottle::ThrottleCheckResult result =
      helper->WillProcessMainFrameUnauthorizedResponse(&handle);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

// Tests that proxy authentication challenges are processed for prompts even
// when the proxy challenger host differs from the navigation URL.
TEST_F(LoginHandlerWithWebContentsTest, ProxyChallengerCancelsForPrompt) {
  LoginTabHelper::CreateForWebContents(web_contents());
  LoginTabHelper* helper = LoginTabHelper::FromWebContents(web_contents());

  net::AuthChallengeInfo challenge;
  challenge.is_proxy = true;
  challenge.challenger = url::SchemeHostPort(GURL("http://proxy.test:8080"));

  content::MockNavigationHandle handle;
  handle.set_url(GURL("https://requested.test/page"));
  handle.SetAuthChallengeInfo(challenge);
  handle.set_global_request_id({network::OriginatingProcessId::browser(), 1});

  content::NavigationThrottle::ThrottleCheckResult result =
      helper->WillProcessMainFrameUnauthorizedResponse(&handle);
  EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
}
