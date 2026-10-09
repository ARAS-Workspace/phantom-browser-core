// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/management/management_api.h"

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_app_deprecation.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_message_listener.h"


#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace keys = extension_management_api_constants;

namespace extensions {

namespace test_utils = api_test_utils;

class ExtensionManagementApiBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionManagementApiBrowserTest() = default;
  ~ExtensionManagementApiBrowserTest() override = default;
  ExtensionManagementApiBrowserTest(const ExtensionManagementApiBrowserTest&) =
      delete;
  ExtensionManagementApiBrowserTest& operator=(
      const ExtensionManagementApiBrowserTest&) = delete;

 protected:
  bool CrashEnabledExtension(const ExtensionId& extension_id) {
    ExtensionHost* background_host =
        ProcessManager::Get(profile())->GetBackgroundHostForExtension(
            extension_id);
    if (!background_host) {
      return false;
    }
    content::CrashTab(background_host->host_contents());
    return true;
  }

 private:
  ScopedInstallVerifierBypassForTest install_verifier_bypass_;
};

class ExtensionManagementApiTestWithBackgroundType
    : public ExtensionManagementApiBrowserTest {
 public:
  ExtensionManagementApiTestWithBackgroundType() {
    // Android does not support Chrome apps and does not have access to the
    // variable g_enable_chrome_apps_for_testing.
    enable_chrome_apps_ = std::make_unique<base::AutoReset<bool>>(
        &extensions::testing::g_enable_chrome_apps_for_testing, true);
  }
  ~ExtensionManagementApiTestWithBackgroundType() override = default;
  ExtensionManagementApiTestWithBackgroundType(
      const ExtensionManagementApiTestWithBackgroundType&) = delete;
  ExtensionManagementApiTestWithBackgroundType& operator=(
      const ExtensionManagementApiTestWithBackgroundType&) = delete;

 private:
  std::unique_ptr<base::AutoReset<bool>> enable_chrome_apps_;
};

// We test this here instead of in an ExtensionApiTest because normal extensions
// are not allowed to call the install function.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       InstallEvent) {
  ExtensionTestMessageListener listener1("ready");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/install_event")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener listener2("got_event");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/management/enabled_extension"),
      {.context_type = ContextType::kFromManifest}));
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       SelfUninstall) {
  // Wait for the helper script to finish before loading the primary
  // extension. This ensures that the onUninstall event listener is
  // added before we proceed to the uninstall step.
  ExtensionTestMessageListener listener1("ready");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/self_uninstall_helper")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
  ExtensionTestMessageListener listener2("success");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/self_uninstall")));
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       SelfUninstallNoPermissions) {
  // Wait for the helper script to finish before loading the primary
  // extension. This ensures that the onUninstall event listener is
  // added before we proceed to the uninstall step.
  ExtensionTestMessageListener listener1("ready");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/self_uninstall_helper")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
  ExtensionTestMessageListener listener2("success");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/self_uninstall_noperm")));
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType, Get) {
  ExtensionTestMessageListener listener("success");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/simple_extension"),
                    {.context_type = ContextType::kFromManifest}));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("management/get")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       GetSelfNoPermissions) {
  ExtensionTestMessageListener listener1("success");
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("management/get_self")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest,
                       GetAllIncludesTerminated) {
  // Load an extension with a background page, so that we know it has a process
  // running.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("management/install_event"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // The management API should list this extension.
  scoped_refptr<ManagementGetAllFunction> function =
      base::MakeRefCounted<ManagementGetAllFunction>();
  std::optional<base::Value> result =
      test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                   profile());
  ASSERT_TRUE(result->is_list());
  EXPECT_EQ(1U, result->GetList().size());

  // And it should continue to do so even after it crashes.
  ASSERT_TRUE(CrashEnabledExtension(extension->id()));

  function = base::MakeRefCounted<ManagementGetAllFunction>();
  result = test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                        profile());
  ASSERT_TRUE(result->is_list());
  EXPECT_EQ(1U, result->GetList().size());
}

class ExtensionManagementApiEscalationTest :
    public ExtensionManagementApiBrowserTest {
 protected:
  // The id of the permissions escalation test extension we use.
  static const char kId[];

  void SetUpOnMainThread() override {
    ExtensionManagementApiBrowserTest::SetUpOnMainThread();
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    base::FilePath pem_path = test_data_dir_.
        AppendASCII("permissions_increase").AppendASCII("permissions.pem");
    base::FilePath path_v1 = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v1"),
        scoped_temp_dir_.GetPath().AppendASCII("permissions1.crx"), pem_path,
        base::FilePath());
    base::FilePath path_v2 = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v2"),
        scoped_temp_dir_.GetPath().AppendASCII("permissions2.crx"), pem_path,
        base::FilePath());

    // Install low-permission version of the extension.
    ASSERT_TRUE(InstallExtension(path_v1, 1));
    EXPECT_TRUE(extension_registry()->enabled_extensions().GetByID(kId));

    // Update to a high-permission version - it should get disabled.
    EXPECT_FALSE(UpdateExtension(kId, path_v2, -1));
    EXPECT_FALSE(extension_registry()->enabled_extensions().GetByID(kId));
    EXPECT_TRUE(extension_registry()->disabled_extensions().GetByID(kId));
    EXPECT_TRUE(
        ExtensionPrefs::Get(profile())->DidExtensionEscalatePermissions(kId));
  }

  void SetEnabled(bool enabled,
                  bool user_gesture,
                  const std::string& expected_error,
                  scoped_refptr<const Extension> extension) {
    scoped_refptr<ManagementSetEnabledFunction> function(
        new ManagementSetEnabledFunction);
    function->set_extension(extension);
    if (user_gesture) {
      function->set_user_gesture(true);
    }
    function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());
    bool response = test_utils::RunFunction(
        function.get(),
        base::StringPrintf("[\"%s\", %s]", kId, base::ToString(enabled)),
        profile(), api_test_utils::FunctionMode::kNone);
    if (expected_error.empty()) {
      EXPECT_EQ(true, response);
    } else {
      EXPECT_TRUE(response == false);
      EXPECT_EQ(expected_error, function->GetError());
    }
  }


 private:
  base::ScopedTempDir scoped_temp_dir_;
};

const char ExtensionManagementApiEscalationTest::kId[] =
    "pgdpcfcocojkjfbgpiianjngphoopgmo";

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiEscalationTest,
                       DisabledReason) {
  scoped_refptr<ManagementGetFunction> function =
      new ManagementGetFunction();
  base::DictValue dict =
      test_utils::ToDict(test_utils::RunFunctionAndReturnSingleResult(
          function.get(), base::StringPrintf("[\"%s\"]", kId), profile()));
  std::string reason =
      api_test_utils::GetString(dict, keys::kDisabledReasonKey);
  EXPECT_TRUE(base::IsStringASCII(reason));
  EXPECT_EQ(reason, std::string(keys::kDisabledReasonPermissionsIncrease));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiEscalationTest,
                       SetEnabled) {
  scoped_refptr<const Extension> source_extension =
      ExtensionBuilder("test").Build();

  // Expect an error about no gesture.
  SetEnabled(true, false, keys::kGestureNeededForEscalationError,
             source_extension);

  {
    // Expect an error that user cancelled the dialog.
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    SetEnabled(true, true, keys::kUserDidNotReEnableError, source_extension);
  }

  {
    // The extension should load when the user accepts the dialog, triggering
    // a new ExtensionHost creation.
    ExtensionHostTestHelper host_helper(profile(), kId);
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    SetEnabled(true, true, std::string(), source_extension);
    host_helper.WaitForRenderProcessReady();
  }

  {
    // Crash the extension. Mock a reload by disabling and then enabling. The
    // extension should be reloaded and enabled.
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    ASSERT_TRUE(CrashEnabledExtension(kId));
    // Register the target extension with extension service.
    scoped_refptr<const Extension> target_extension =
        ExtensionBuilder("TargetExtension").SetID(kId).Build();
    extension_registrar()->AddExtension(target_extension);
    SetEnabled(false, true, std::string(), source_extension);
    SetEnabled(true, true, std::string(), source_extension);
    EXPECT_TRUE(extension_registry()->enabled_extensions().GetByID(kId));
  }
}

}  // namespace extensions
