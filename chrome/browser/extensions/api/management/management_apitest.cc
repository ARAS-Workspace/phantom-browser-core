// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/auto_reset.h"
#include "base/test/gtest_tags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_app_deprecation.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/manifest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using extensions::Extension;
using extensions::Manifest;
using extensions::mojom::ManifestLocation;

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
bool ExpectChromeAppsDefaultEnabled() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return false;
#else
  return true;
#endif
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

class ExtensionManagementApiTest : public extensions::ExtensionApiTest {
 public:
  ExtensionManagementApiTest() {
    enable_chrome_apps_ = std::make_unique<base::AutoReset<bool>>(
        &extensions::testing::g_enable_chrome_apps_for_testing, true);
  }
  ~ExtensionManagementApiTest() override = default;
  ExtensionManagementApiTest& operator=(const ExtensionManagementApiTest&) =
      delete;
  ExtensionManagementApiTest(const ExtensionManagementApiTest&) = delete;

 protected:
  void LoadExtensions() {
    base::FilePath basedir = test_data_dir_.AppendASCII("management");

    // Load 5 enabled items.
    LoadNamedExtension(basedir, "enabled_extension");
    LoadNamedExtension(basedir, "enabled_app");
    LoadNamedExtension(basedir, "description");
    LoadNamedExtension(basedir, "permissions");
    LoadNamedExtension(basedir, "short_name");

    // Load 2 disabled items.
    LoadNamedExtension(basedir, "disabled_extension");
    DisableExtension(extension_ids_["disabled_extension"]);
    LoadNamedExtension(basedir, "disabled_app");
    DisableExtension(extension_ids_["disabled_app"]);
  }

  void LoadNamedExtension(const base::FilePath& path,
                          const std::string& name) {
    const Extension* extension = LoadExtension(
        path.AppendASCII(name), {.context_type = ContextType::kFromManifest});
    ASSERT_TRUE(extension);
    extension_ids_[name] = extension->id();
  }

  void InstallNamedExtension(const base::FilePath& path,
                             const std::string& name,
                             ManifestLocation install_source) {
    const Extension* extension = InstallExtension(path.AppendASCII(name), 1,
                                                  install_source);
    ASSERT_TRUE(extension);
    extension_ids_[name] = extension->id();
  }

  // Maps installed extension names to their IDs.
  std::map<std::string, std::string> extension_ids_;

 protected:
  std::unique_ptr<base::AutoReset<bool>> enable_chrome_apps_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, Basics) {
  // Android does not provide the XmlUnitTestResultPrinter this method needs.
  base::AddFeatureIdTagToTestResult(
      "screenplay-7a245632-83b2-4dc8-a1db-283ef595e2df");

  LoadExtensions();

  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  InstallNamedExtension(basedir, "internal_extension",
                        ManifestLocation::kInternal);
  InstallNamedExtension(basedir, "external_extension",
                        ManifestLocation::kExternalPref);
  InstallNamedExtension(basedir, "admin_extension",
                        ManifestLocation::kExternalPolicyDownload);
  InstallNamedExtension(basedir, "version_name", ManifestLocation::kInternal);

  ASSERT_TRUE(RunExtensionTest("management/basics"));
}

#define MAYBE_NoPermission NoPermission
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, MAYBE_NoPermission) {
  LoadExtensions();
  ASSERT_TRUE(RunExtensionTest("management/no_permission"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, Uninstall) {
  LoadExtensions();
  // Confirmation dialog will be shown for uninstallations except for self.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  ASSERT_TRUE(RunExtensionTest("management/uninstall"));
}

// Skipped on Android because it does not support Chrome apps.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, CreateAppShortcut) {
  LoadExtensions();
  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  LoadNamedExtension(basedir, "packaged_app");

  extensions::ManagementCreateAppShortcutFunction::SetAutoConfirmForTest(true);
  ASSERT_TRUE(RunExtensionTest("management/create_app_shortcut"));
}

// Tests actions on extensions when no management policy is in place.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, ManagementPolicyAllowed) {
  LoadExtensions();
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(
      extension_ids_["enabled_extension"]));

  // Ensure that all actions are allowed.
  extensions::ExtensionSystem::Get(profile())
      ->management_policy()
      ->UnregisterAllProviders();

  ASSERT_TRUE(RunExtensionTest("management/management_policy",
                               {.custom_arg = "runAllowedTests"}));
  // The last thing the test does is uninstall the "enabled_extension".
  EXPECT_FALSE(
      registry->GetExtensionById(extension_ids_["enabled_extension"],
                                 extensions::ExtensionRegistry::EVERYTHING));
}

// Tests actions on extensions when management policy prohibits those actions.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, ManagementPolicyProhibited) {
  LoadExtensions();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(
      extension_ids_["enabled_extension"]));

  // Prohibit status changes.
  extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile())->management_policy();
  policy->UnregisterAllProviders();
  extensions::TestManagementPolicyProvider provider(
      extensions::TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS |
      extensions::TestManagementPolicyProvider::MUST_REMAIN_ENABLED |
      extensions::TestManagementPolicyProvider::MUST_REMAIN_INSTALLED);
  policy->RegisterProvider(&provider);
  ASSERT_TRUE(RunExtensionTest("management/management_policy",
                               {.custom_arg = "runProhibitedTests"}));
}

// Skipped on Android because it does not support Chrome apps.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest,
                       NoLaunchPanelAppsDeprecated) {
  extensions::testing::g_enable_chrome_apps_for_testing = false;
  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded");
  auto* extension =
      LoadExtension(test_data_dir_.AppendASCII("management/launch_on_install"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Load an app with app.launch.container = "panel". This is a chrome app, so
  // it shouldn't be launched where that functionality has been deprecated.
  ExtensionTestMessageListener launched_app("launched app");
  ExtensionTestMessageListener chrome_apps_error("got_chrome_apps_error");
  auto* app =
      LoadExtension(test_data_dir_.AppendASCII("management/launch_app_panel"),
                    {.context_type = ContextType::kFromManifest});
  ASSERT_TRUE(app);

  if (ExpectChromeAppsDefaultEnabled()) {
    EXPECT_TRUE(launched_app.WaitUntilSatisfied());
    EXPECT_FALSE(chrome_apps_error.was_satisfied());
  } else {
    EXPECT_TRUE(chrome_apps_error.WaitUntilSatisfied());
    EXPECT_FALSE(launched_app.was_satisfied());
  }
}

// Skipped on Android because it does not support Chrome apps.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, NoLaunchTabAppDeprecated) {
  extensions::testing::g_enable_chrome_apps_for_testing = false;
  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Code below assumes that the test starts with a single browser window
  // hosting one tab.
  ASSERT_EQ(1u, extensions::browsertest_util::GetWindowControllerCountInProfile(
                    profile()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Load an app with app.launch.container = "tab". This is a chrome app, so
  // it shouldn't be launched where that functionality has been deprecated.
  ExtensionTestMessageListener launched_app("launched app");
  ExtensionTestMessageListener chrome_apps_error("got_chrome_apps_error");
  auto* app =
      LoadExtension(test_data_dir_.AppendASCII("management/launch_app_tab"),
                    {.context_type = ContextType::kFromManifest});
  ASSERT_TRUE(app);

  if (ExpectChromeAppsDefaultEnabled()) {
    EXPECT_TRUE(launched_app.WaitUntilSatisfied());
    EXPECT_FALSE(chrome_apps_error.was_satisfied());
  } else {
    EXPECT_TRUE(chrome_apps_error.WaitUntilSatisfied());
    EXPECT_FALSE(launched_app.was_satisfied());
  }
}

// Flaky on MacOS: crbug.com/41431910
#if BUILDFLAG(IS_MAC)
#define MAYBE_LaunchType DISABLED_LaunchType
#else
#define MAYBE_LaunchType LaunchType
#endif
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, MAYBE_LaunchType) {
  LoadExtensions();
  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  LoadNamedExtension(basedir, "packaged_app");

  ASSERT_TRUE(RunExtensionTest("management/launch_type"));
}
