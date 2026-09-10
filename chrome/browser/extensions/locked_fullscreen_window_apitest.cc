// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

using ::testing::NotNull;

namespace extensions {
namespace {

class LockedFullscreenWindowApiTestBase : public ExtensionApiTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

using LockedFullscreenWindowApiTestNonChromeOS =
    LockedFullscreenWindowApiTestBase;

// Loading an extension requiring the 'lockWindowFullscreenPrivate' permission
// on non Chrome OS platforms should always fail since the API is available only
// on Chrome OS.
IN_PROC_BROWSER_TEST_F(LockedFullscreenWindowApiTestNonChromeOS,
                       OpenLockedFullscreenWindow) {
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("locked_fullscreen/with_permission"),
      {.ignore_manifest_warnings = true});
  ASSERT_TRUE(extension);
  ASSERT_EQ(1u, extension->install_warnings().size());
  EXPECT_EQ(std::string("'lockWindowFullscreenPrivate' "
                        "is not allowed for specified platform."),
            extension->install_warnings()[0].message);
}

}  // namespace
}  // namespace extensions
