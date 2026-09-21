// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class ShortcutsVersioningMacTest : public WebAppTest {
 public:
  ShortcutsVersioningMacTest()
      : WebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    WebAppTest::SetUp();

    // Put shortcuts somewhere under the home dir, as otherwise LaunchServices
    // won't be able to find them.
    override_registration_ =
        OsIntegrationTestOverrideImpl::OverrideForTesting();

    // These tests require a real OsIntegrationManager, rather than the
    // FakeOsIntegrationManager that is created by default.
    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    fake_provider().SetOsIntegrationManager(
        std::make_unique<OsIntegrationManager>(
            profile(), std::move(file_handler_manager),
            std::move(protocol_handler_manager)));

    // Do not yet start WebAppProvider here in SetUp, as tests verify behavior
    // that happens during and is triggered by start. As such individual tests
    // start the WebAppProvider when they need to.
  }

  void TearDown() override {
    OsIntegrationManager::SetUpdateShortcutsForAllAppsCallback(
        base::NullCallback());

    override_registration_.reset();

    WebAppTest::TearDown();
  }

  base::FilePath GetShortcutPath(const std::string& app_name) {
    std::string shortcut_filename = app_name + ".app";
    return override_registration_->test_override()
        .chrome_apps_folder()
        .AppendASCII(shortcut_filename);
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  const char* kTestApp1Name = "test app";
  const GURL kTestApp1Url = GURL("https://foobar.com");
  const char* kTestApp2Name = "example app";
  const GURL kTestApp2Url = GURL("https://example.com");

 private:
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

TEST_F(ShortcutsVersioningMacTest, InitialVersionIsStored) {
  // Starting the WebAppProvider, and more importantly its OsIntegrationManager
  // subsystem should cause the current shortcuts version to be written to
  // prefs.
  EXPECT_FALSE(profile()->GetPrefs()->HasPrefPath(prefs::kAppShortcutsVersion));
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kAppShortcutsOsVersion));
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(prefs::kAppShortcutsVersion));
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(prefs::kAppShortcutsArch));
  EXPECT_TRUE(
      profile()->GetPrefs()->HasPrefPath(prefs::kAppShortcutsOsVersion));
}

}  // namespace web_app
