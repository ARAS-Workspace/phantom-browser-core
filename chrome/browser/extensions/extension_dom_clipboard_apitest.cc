// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/script_executor.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

class ClipboardApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

};

}  // namespace

// Flaky on Mac. See https://crbug.com/40195042.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Extension DISABLED_Extension
#else
#define MAYBE_Extension Extension
#endif
IN_PROC_BROWSER_TEST_F(ClipboardApiTest, MAYBE_Extension) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("clipboard/extension")) << message_;
}

// Flaky on Mac. See https://crbug.com/40600305.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ExtensionNoPermission DISABLED_ExtensionNoPermission
#else
#define MAYBE_ExtensionNoPermission ExtensionNoPermission
#endif
IN_PROC_BROWSER_TEST_F(ClipboardApiTest, MAYBE_ExtensionNoPermission) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("clipboard/extension_no_permission"))
      << message_;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Regression test for crbug.com/40051481
// TODO(crbug.com/496276762): Fix on desktop Android. IsClipboardPasteAllowed()
// always returns true, because it thinks there was a recent user interaction.
IN_PROC_BROWSER_TEST_F(ClipboardApiTest, BrowserPermissionCheck) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("/english_page.html")));
  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();
  // No extensions are installed. Clipboard access should be disallowed.
  EXPECT_FALSE(
      content::GetContentClientForTesting()->browser()->IsClipboardPasteAllowed(
          render_frame_host));

  static constexpr char kManifest[] =
      R"({
         "name": "Ext",
         "manifest_version": 3,
         "version": "1",
         "background": {"service_worker": "background.js"},
         "permissions": ["scripting", "clipboardRead"],
         "host_permissions": ["<all_urls>"]
       })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// blank ");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Even with an extension installed, clipboard access is disallowed for
  // the page.
  EXPECT_FALSE(
      content::GetContentClientForTesting()->browser()->IsClipboardPasteAllowed(
          render_frame_host));

  // Inject a script on the page through the extension.
  static constexpr char kScript[] =
      R"(
       (async () => {
         let tabs = await chrome.tabs.query({active: true});
         await chrome.scripting.executeScript(
             {target: {tabId: tabs[0].id},
             func: function() {}} );
         chrome.test.sendScriptResult('done');
       })();)";

  // This will execute the script and wait for it to complete, ensuring
  // the browser is aware of the executing content script.
  BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(), kScript,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  // Now the page should have access to the clipboard.
  EXPECT_TRUE(
      content::GetContentClientForTesting()->browser()->IsClipboardPasteAllowed(
          render_frame_host));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace extensions
