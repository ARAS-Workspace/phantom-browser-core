// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/crl_set.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "ui/base/window_open_disposition.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"

namespace {

size_t GetDisclosureCount(content::WebContents* contents) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(contents);
  return infobar_manager ? infobar_manager->infobars().size() : 0;
}

infobars::InfoBar* GetInfobar(content::WebContents* contents) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(contents);
  DCHECK(infobar_manager);
  return infobar_manager->infobars()[0];
}

// Simulates dismissing the infobar (e.g., clicking the 'X' close button).
// This triggers the dismissal callback which activates the cooldown.
//
// We use InfoBarDismissed() instead of Accept() because this infobar has no
// action buttons on Desktop. While Accept() worked as a fallback in the legacy
// delegate, the new framework only binds the cooldown activation to the
// dismissal path (since there is no OK button to Accept). Calling
// InfoBarDismissed() works correctly for both the legacy and migrated paths.
void CloseDisclosure(content::WebContents* contents) {
  infobars::InfoBar* infobar = GetInfobar(contents);
  if (!infobar) {
    return;
  }

  infobar->delegate()->InfoBarDismissed();
  infobar->RemoveSelf();
}

}  // namespace

class KnownInterceptionDisclosurePlatformBrowserTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  KnownInterceptionDisclosurePlatformBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    if (GetParam()) {
      feature_list_.InitAndEnableFeatureWithParameters(
          infobars::kCentralizedInfoBarFramework,
          {{"MigratedKnownInterceptionDisclosure", "true"}});
    } else {
      feature_list_.InitAndDisableFeature(
          infobars::kCentralizedInfoBarFramework);
    }
  }

  KnownInterceptionDisclosurePlatformBrowserTest(
      const KnownInterceptionDisclosurePlatformBrowserTest&) = delete;
  KnownInterceptionDisclosurePlatformBrowserTest& operator=(
      const KnownInterceptionDisclosurePlatformBrowserTest&) = delete;

  void SetUp() override { PlatformBrowserTest::SetUp(); }

  void TearDown() override { PlatformBrowserTest::TearDown(); }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(https_server_.Start());

    // Load a CRLSet that marks the root as a known MITM.
    std::string crl_set_bytes;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::ReadFileToString(net::GetTestCertsDirectory().AppendASCII(
                                 "crlset_known_interception_by_root.raw"),
                             &crl_set_bytes);
    }
    base::RunLoop run_loop;
    content::GetCertVerifierServiceFactory()->UpdateCRLSet(
        base::as_byte_span(crl_set_bytes), run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(KnownInterceptionDisclosurePlatformBrowserTest,
                       DisclosureTriggerSmokeTest) {
  const GURL kInterceptedUrl(https_server_.GetURL("/ssl/google.html"));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);

  // Trigger showing the disclosure.
  ASSERT_TRUE(content::NavigateToURL(tab, kInterceptedUrl));
}

IN_PROC_BROWSER_TEST_P(KnownInterceptionDisclosurePlatformBrowserTest,
                       OnlyShowDisclosureOncePerSession) {
  const GURL kInterceptedUrl(https_server_.GetURL("/ssl/google.html"));

  content::WebContents* tab1 = chrome_test_utils::GetActiveWebContents(this);

  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  clock_ptr->SetNow(base::Time::Now());
  KnownInterceptionDisclosureCooldown::GetInstance()->SetClockForTesting(
      std::move(clock));

  // Trigger the disclosure infobar by navigating to a page served by a root
  // marked as known interception.
  ASSERT_TRUE(content::NavigateToURL(tab1, kInterceptedUrl));
  EXPECT_EQ(1u, GetDisclosureCount(tab1));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  // Test that the infobar is shown on new tabs after it has been triggered
  // once.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* tab2 = tab_strip_model->GetActiveWebContents();
  EXPECT_EQ(1u, GetDisclosureCount(tab2));

  // Close the new tab.
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                      TabCloseTypes::CLOSE_USER_GESTURE);

  // Reload the first page -- infobar should still show.
  ASSERT_TRUE(content::NavigateToURL(tab1, kInterceptedUrl));
  EXPECT_EQ(1u, GetDisclosureCount(tab1));

  // Dismiss the disclosure.
  CloseDisclosure(tab1);
  EXPECT_EQ(0u, GetDisclosureCount(tab1));

  // Try to trigger again by reloading the page -- disclosure should not show.
  ASSERT_TRUE(content::NavigateToURL(tab1, kInterceptedUrl));
  EXPECT_EQ(0u, GetDisclosureCount(tab1));

  // Move clock ahead 8 days.
  clock_ptr->Advance(base::Days(8));

  // Trigger the disclosure again -- disclosure should show again.
  ASSERT_TRUE(content::NavigateToURL(tab1, kInterceptedUrl));
  EXPECT_EQ(1u, GetDisclosureCount(tab1));
}

IN_PROC_BROWSER_TEST_P(KnownInterceptionDisclosurePlatformBrowserTest,
                       PRE_CooldownResetsOnBrowserRestart) {
  const GURL kInterceptedUrl(https_server_.GetURL("/ssl/google.html"));

  // Trigger the disclosure.
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(tab, kInterceptedUrl));
  EXPECT_EQ(1u, GetDisclosureCount(tab));

  // Dismiss the disclosure.
  CloseDisclosure(tab);
  EXPECT_EQ(0u, GetDisclosureCount(tab));
}

IN_PROC_BROWSER_TEST_P(KnownInterceptionDisclosurePlatformBrowserTest,
                       CooldownResetsOnBrowserRestart) {
  const GURL kInterceptedUrl(https_server_.GetURL("/ssl/google.html"));

  // On restart, no disclosure should be shown initially.
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  EXPECT_EQ(0u, GetDisclosureCount(tab));

  // Triggering the disclosure again after browser restart should show
  // the infobar (the cooldown period should no longer apply on Desktop).
  ASSERT_TRUE(content::NavigateToURL(tab, kInterceptedUrl));
  EXPECT_EQ(1u, GetDisclosureCount(tab));
}

INSTANTIATE_TEST_SUITE_P(All,
                         KnownInterceptionDisclosurePlatformBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Migrated" : "Legacy";
                         });
