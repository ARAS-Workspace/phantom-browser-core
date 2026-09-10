// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using WebrtcLoggingPrivateExtensionApiTest = extensions::ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateExtensionApiTest,
                       TestNoGetLogsDirectoryPermissionsFromHangoutsExtension) {
  ASSERT_TRUE(RunExtensionTest(
      "webrtc_logging_private/no_get_logs_directory_permissions", {},
      {.load_as_component = true}))
      << message_;
}

// The following tests are executed as Chrome Apps, which are only supported on
// ChromeOS.
