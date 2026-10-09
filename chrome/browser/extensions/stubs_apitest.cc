// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// Tests that we throw errors when you try using extension APIs that aren't
// supported in content scripts. If this test causes a renderer crash on one
// platform, you may be including that platform in _api_features.json but not
// including your API JSON file in schema.gni for the right config.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Stubs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(RunExtensionTest("stubs")) << message_;

  ResultCatcher catcher;

  // Navigate to a simple http:// page, which should get the content script
  // injected and run the rest of the test.
  GURL url(embedded_test_server()->GetURL("/extensions/test_file.html"));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

  ASSERT_TRUE(catcher.GetNextResult());
}

}  // namespace extensions
