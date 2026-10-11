// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/management/management_api.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/management.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/permission_set.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

constexpr char kUninstallSourceHistogramName[] = "Extensions.UninstallSource";

std::unique_ptr<KeyedService> BuildManagementApi(
    content::BrowserContext* context) {
  return std::make_unique<ManagementAPI>(context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<extensions::EventRouter>(
      profile, ExtensionPrefs::Get(profile));
}

namespace constants = extension_management_api_constants;

// TODO(devlin): Unittests are awesome. Test more with unittests and less with
// heavy api/browser tests.
class ManagementApiUnitTest : public ExtensionServiceTestWithInstall {
 public:
  ManagementApiUnitTest(const ManagementApiUnitTest&) = delete;
  ManagementApiUnitTest& operator=(const ManagementApiUnitTest&) = delete;

 protected:
  ManagementApiUnitTest() = default;
  ~ManagementApiUnitTest() override = default;

  // A wrapper around api_test_utils::RunFunction that runs with
  // the associated browser, no flags, and can take stack-allocated arguments.
  bool RunFunction(const scoped_refptr<ExtensionFunction>& function,
                   const base::ListValue& args);

  // Runs the management.setEnabled() function to enable an extension.
  bool RunSetEnabledFunction(content::WebContents* web_contents,
                             const ExtensionId& extension_id,
                             bool use_user_gesture,
                             bool accept_dialog,
                             std::string* error,
                             bool enabled = true);

  // Returns the initialization parameters for the extension service.
  virtual ExtensionServiceInitParams GetExtensionServiceInitParams() {
    return ExtensionServiceInitParams();
  }

  // ExtensionServiceTestWithInstall:
  void SetUp() override;

 private:
  // This test does not create a root window. Because of this,
  // ScopedDisableRootChecking needs to be used (which disables the root window
  // check).
  test::ScopedDisableRootChecking disable_root_checking_;
};

bool ManagementApiUnitTest::RunFunction(
    const scoped_refptr<ExtensionFunction>& function,
    const base::ListValue& args) {
  return api_test_utils::RunFunction(function.get(), args.Clone(), profile(),
                                     api_test_utils::FunctionMode::kNone);
}

bool ManagementApiUnitTest::RunSetEnabledFunction(
    content::WebContents* web_contents,
    const ExtensionId& extension_id,
    bool use_user_gesture,
    bool accept_dialog,
    std::string* error,
    bool enabled) {
  ScopedTestDialogAutoConfirm auto_confirm(
      accept_dialog ? ScopedTestDialogAutoConfirm::ACCEPT
                    : ScopedTestDialogAutoConfirm::CANCEL);
  std::optional<ExtensionFunction::ScopedUserGestureForTests> gesture =
      std::nullopt;
  if (use_user_gesture) {
    gesture.emplace();
  }
  auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
  if (web_contents) {
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  }
  base::ListValue args;
  args.Append(extension_id);
  args.Append(enabled);
  bool result = RunFunction(function, args);
  if (error) {
    *error = function->GetError();
  }
  return result;
}

void ManagementApiUnitTest::SetUp() {
  ExtensionServiceTestWithInstall::SetUp();
  InitializeExtensionService(GetExtensionServiceInitParams());
  ManagementAPI::GetFactoryInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildManagementApi));

  EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));
}

// Test the basic parts of management.setEnabled.
TEST_F(ManagementApiUnitTest, ManagementSetEnabled) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  registrar()->AddExtension(extension.get());
  scoped_refptr<const Extension> source_extension =
      ExtensionBuilder("Test").Build();
  registrar()->AddExtension(source_extension.get());
  const ExtensionId& extension_id = extension->id();
  auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
  function->set_extension(source_extension);

  base::ListValue disable_args;
  disable_args.Append(extension_id);
  disable_args.Append(false);

  // Test disabling an (enabled) extension.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
  EXPECT_TRUE(RunFunction(function, disable_args)) << function->GetError();
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

  base::ListValue enable_args;
  enable_args.Append(extension_id);
  enable_args.Append(true);

  // Test re-enabling it.
  function = base::MakeRefCounted<ManagementSetEnabledFunction>();
  EXPECT_TRUE(RunFunction(function, enable_args)) << function->GetError();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));

  // Test that the enable function checks management policy, so that we can't
  // disable an extension that is required.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->RegisterProvider(&provider);

  function = base::MakeRefCounted<ManagementSetEnabledFunction>();
  EXPECT_FALSE(RunFunction(function, disable_args));
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUserCantModifyError,
                                           extension_id),
            function->GetError());
  policy->UnregisterProvider(&provider);
}

// chrome.management.setEnabled can be called with or without a user gesture.
// Verify that the associated histogram is set accordingly.
TEST_F(ManagementApiUnitTest, ManagementSetEnabledMaybeHasUserGesture) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  registrar()->AddExtension(extension.get());
  auto run_set_enabled_function = [&](bool use_user_gesture) {
    bool success = RunSetEnabledFunction(
        /*web_contents=*/nullptr, extension->id(), use_user_gesture,
        /*accept_dialog=*/true, nullptr);
    ASSERT_TRUE(success);
    histogram_tester.ExpectBucketCount(
        ManagementSetEnabledFunction::kSetEnabledHasUserGestureHistogramName,
        use_user_gesture, 1);
  };

  run_set_enabled_function(/*use_user_gesture=*/true);
  run_set_enabled_function(/*use_user_gesture=*/false);
}

// Test that component extensions cannot be disabled, and that policy extensions
// can be disabled only by component/policy extensions.
TEST_F(ManagementApiUnitTest, ComponentPolicyDisabling) {
  auto component = ExtensionBuilder("component")
                       .SetLocation(ManifestLocation::kComponent)
                       .Build();
  auto component2 = ExtensionBuilder("component2")
                        .SetLocation(ManifestLocation::kComponent)
                        .Build();
  auto policy = ExtensionBuilder("policy")
                    .SetLocation(ManifestLocation::kExternalPolicy)
                    .Build();
  auto policy2 = ExtensionBuilder("policy2")
                     .SetLocation(ManifestLocation::kExternalPolicy)
                     .Build();
  auto internal = ExtensionBuilder("internal")
                      .SetLocation(ManifestLocation::kInternal)
                      .Build();

  registrar()->AddExtension(component.get());
  registrar()->AddExtension(component2.get());
  registrar()->AddExtension(policy.get());
  registrar()->AddExtension(policy2.get());
  registrar()->AddExtension(internal.get());

  auto extension_can_disable_extension =
      [this](scoped_refptr<const Extension> source_extension,
             scoped_refptr<const Extension> target_extension) {
        const ExtensionId& id = target_extension->id();
        base::ListValue args;
        args.Append(id);
        args.Append(false /* disable the extension */);
        auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
        function->set_extension(source_extension);
        bool did_disable = RunFunction(function, args);
        // If the extension was disabled, re-enable it.
        if (did_disable) {
          EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
          registrar()->EnableExtension(id);
        } else {
          EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
        }
        return did_disable;
      };

  // Component extension cannot be disabled.
  EXPECT_FALSE(extension_can_disable_extension(component2, component));
  EXPECT_FALSE(extension_can_disable_extension(policy, component));
  EXPECT_FALSE(extension_can_disable_extension(internal, component));

  // Policy extension can be disabled by component/policy extensions, but not
  // others.
  EXPECT_TRUE(extension_can_disable_extension(component, policy));
  EXPECT_TRUE(extension_can_disable_extension(policy2, policy));
  EXPECT_FALSE(extension_can_disable_extension(internal, policy));
}

// Test that policy extensions can be enabled only by component/policy
// extensions.
TEST_F(ManagementApiUnitTest, ComponentPolicyEnabling) {
  auto component = ExtensionBuilder("component")
                       .SetLocation(ManifestLocation::kComponent)
                       .Build();
  auto policy = ExtensionBuilder("policy")
                    .SetLocation(ManifestLocation::kExternalPolicy)
                    .Build();
  auto policy2 = ExtensionBuilder("policy2")
                     .SetLocation(ManifestLocation::kExternalPolicy)
                     .Build();
  auto internal = ExtensionBuilder("internal")
                      .SetLocation(ManifestLocation::kInternal)
                      .Build();

  registrar()->AddExtension(component.get());
  registrar()->AddExtension(policy.get());
  registrar()->AddExtension(policy2.get());
  registrar()->AddExtension(internal.get());
  registrar()->DisableExtensionWithSource(
      component.get(), policy->id(), disable_reason::DISABLE_BLOCKED_BY_POLICY);

  auto extension_can_enable_extension =
      [this, component](scoped_refptr<const Extension> source_extension,
                        scoped_refptr<const Extension> target_extension) {
        const ExtensionId& id = target_extension->id();
        base::ListValue args;
        args.Append(id);
        args.Append(true /* enable the extension */);
        auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
        function->set_extension(source_extension);
        bool did_enable = RunFunction(function, args);
        // If the extension was enabled, disable it.
        if (did_enable) {
          EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
          registrar()->DisableExtensionWithSource(
              component.get(), id, disable_reason::DISABLE_BLOCKED_BY_POLICY);
        } else {
          EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
        }
        return did_enable;
      };

  // Policy extension can be enabled by component/policy extensions, but not
  // others.
  EXPECT_TRUE(extension_can_enable_extension(component, policy));
  EXPECT_TRUE(extension_can_enable_extension(policy2, policy));
  EXPECT_FALSE(extension_can_enable_extension(internal, policy));
}

// Tests management.uninstall.
TEST_F(ManagementApiUnitTest, ManagementUninstall) {
  // Note: uninstall calls must come from an extension, WebUI or the Webstore.
  // To test default behavior we test calling from a WebUI context, akin to what
  // we would get from the extension management page.
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  registrar()->AddExtension(extension.get());
  const ExtensionId& extension_id = extension->id();

  base::ListValue uninstall_args;
  uninstall_args.Append(extension->id());
  base::HistogramTester tester;

  // Auto-accept any uninstalls.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);

    // Uninstall requires a user gesture, so this should fail.
    auto function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_source_context_type(mojom::ContextType::kWebUi);
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    EXPECT_EQ(std::string(constants::kGestureNeededForUninstallError),
              function->GetError());

    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_source_context_type(mojom::ContextType::kWebUi);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
    tester.ExpectBucketCount(
        kUninstallSourceHistogramName,
        extensions::UNINSTALL_SOURCE_CHROME_EXTENSIONS_PAGE, 1);
  }

  // Install the extension again, and try uninstalling, auto-canceling the
  // dialog.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    registrar()->AddExtension(extension.get());
    scoped_refptr<ExtensionFunction> function =
        base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_source_context_type(mojom::ContextType::kWebUi);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // The uninstall should have failed.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());
    tester.ExpectBucketCount(
        kUninstallSourceHistogramName,
        extensions::UNINSTALL_SOURCE_CHROME_EXTENSIONS_PAGE, 2);

    // Try again, using showConfirmDialog: false.
    base::DictValue options;
    options.Set("showConfirmDialog", false);
    uninstall_args.Append(std::move(options));
    function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_source_context_type(mojom::ContextType::kWebUi);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // This should still fail, since extensions can only suppress the dialog for
    // uninstalling themselves.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());
    tester.ExpectBucketCount(
        kUninstallSourceHistogramName,
        extensions::UNINSTALL_SOURCE_CHROME_EXTENSIONS_PAGE, 3);

    // If we have the extension uninstall itself, the uninstall should succeed
    // (even though we auto-cancel any dialog), because the dialog is never
    // shown.
    uninstall_args.erase(uninstall_args.begin());
    function = base::MakeRefCounted<ManagementUninstallSelfFunction>();
    // Note: this time the source is coming from the extension itself, not a
    // WebUI based context.
    function->set_extension(extension);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
    // Note: No Extensins.UninstallSource bucket is incremented here, as no
    // dialog was shown.
  }
}

// Tests management.uninstall from the Web Store hosted app.
TEST_F(ManagementApiUnitTest, ManagementUninstallWebstoreHostedApp) {
  scoped_refptr<const Extension> triggering_extension =
      ExtensionBuilder("Test").SetID(extensions::kWebStoreAppId).Build();
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  registrar()->AddExtension(extension.get());
  const ExtensionId& extension_id = extension->id();
  base::ListValue uninstall_args;
  uninstall_args.Append(extension->id());
  base::HistogramTester tester;

  {
    auto function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_extension(triggering_extension);

    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // When the dialog is automatically canceled, an error will have been
    // reported to the extension telling it.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());
    tester.ExpectBucketCount(kUninstallSourceHistogramName,
                             extensions::UNINSTALL_SOURCE_CHROME_WEBSTORE, 1);
  }

  {
    auto function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_extension(triggering_extension);

    bool did_show = false;
    auto callback = base::BindRepeating(
        [](bool* did_show) { *did_show = true; }, &did_show);
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(
        &callback);

    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension_id));
    EXPECT_TRUE(did_show);
    tester.ExpectBucketCount(kUninstallSourceHistogramName,
                             extensions::UNINSTALL_SOURCE_CHROME_WEBSTORE, 2);

    // Reset the callback.
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(nullptr);
  }
}

// Tests management.uninstall from the new Webstore domain.
TEST_F(ManagementApiUnitTest, ManagementUninstallNewWebstore) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  registrar()->AddExtension(extension.get());
  const ExtensionId& extension_id = extension->id();
  base::ListValue uninstall_args;
  uninstall_args.Append(extension->id());
  base::HistogramTester tester;

  // Note: no triggering extension is set on the ExtensionFunction, but the
  // associated URL should be from the webstore domain.
  auto function = base::MakeRefCounted<ManagementUninstallFunction>();
  function->set_source_url(GURL(extension_urls::GetNewWebstoreLaunchURL()));

  bool did_show = false;
  auto callback =
      base::BindRepeating([](bool* did_show) { *did_show = true; }, &did_show);
  extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(&callback);

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

  EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
  // The extension should be uninstalled.
  EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension_id));
  EXPECT_TRUE(did_show);
  tester.ExpectBucketCount(kUninstallSourceHistogramName,
                           extensions::UNINSTALL_SOURCE_CHROME_WEBSTORE, 1);

  // Reset the callback.
  extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(nullptr);
}

// Tests management.uninstall from a normal extension, which will create a
// programmatic uninstall dialog that identifies the extension that called it.
TEST_F(ManagementApiUnitTest, ManagementUninstallProgramatic) {
  scoped_refptr<const Extension> triggering_extension =
      ExtensionBuilder("Triggering Extension").SetID("123").Build();
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  registrar()->AddExtension(extension.get());
  const ExtensionId& extension_id = extension->id();
  base::ListValue uninstall_args;
  uninstall_args.Append(extension->id());
  base::HistogramTester tester;
  {
    auto function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_extension(triggering_extension);

    bool did_show = false;
    auto callback = base::BindRepeating(
        [](bool* did_show) { *did_show = true; }, &did_show);
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(
        &callback);

    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension_id));
    EXPECT_TRUE(did_show);
    tester.ExpectBucketCount(kUninstallSourceHistogramName,
                             extensions::UNINSTALL_SOURCE_EXTENSION, 1);

    // Reset the callback.
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(nullptr);
  }
}
// Tests uninstalling a blocklisted extension via management.uninstall.
TEST_F(ManagementApiUnitTest, ManagementUninstallBlocklisted) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  registrar()->AddExtension(extension.get());
  const ExtensionId& id = extension->id();

  service()->BlocklistExtensionForTest(id);
  EXPECT_NE(nullptr, registry()->GetInstalledExtension(id));

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  auto function = base::MakeRefCounted<ManagementUninstallFunction>();
  function->set_source_context_type(mojom::ContextType::kWebUi);
  base::ListValue uninstall_args;
  uninstall_args.Append(id);
  EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();

  EXPECT_EQ(nullptr, registry()->GetInstalledExtension(id));
}

TEST_F(ManagementApiUnitTest, ManagementEnableOrDisableBlocklisted) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  registrar()->AddExtension(extension.get());
  const ExtensionId& id = extension->id();

  service()->BlocklistExtensionForTest(id);
  EXPECT_NE(nullptr, registry()->GetInstalledExtension(id));

  // Test enabling it.
  {
    base::ListValue enable_args;
    enable_args.Append(id);
    enable_args.Append(true);
    auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
    EXPECT_TRUE(RunFunction(function, enable_args)) << function->GetError();
    EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
    EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  }

  // Test disabling it
  {
    base::ListValue disable_args;
    disable_args.Append(id);
    disable_args.Append(false);

    auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
    EXPECT_TRUE(RunFunction(function, disable_args)) << function->GetError();
    EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
    EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  }
}

TEST_F(ManagementApiUnitTest, ExtensionInfo_MayEnable) {
  using ExtensionInfo = api::management::ExtensionInfo;

  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  registrar()->AddExtension(extension.get());

  const std::string args =
      base::StringPrintf("[\"%s\"]", extension->id().c_str());

  // Initially the extension should show as enabled.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  {
    auto function = base::MakeRefCounted<ManagementGetFunction>();
    std::optional<base::Value> value =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), args,
                                                         profile());
    ASSERT_TRUE(value);
    std::optional<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->enabled);
    // |may_enable| is only returned for extensions which are not enabled.
    EXPECT_FALSE(info->may_enable);
  }

  // Simulate blocklisting the extension and verify that the extension shows as
  // disabled with a false value of |may_enable|.
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_LOAD);
  policy->RegisterProvider(&provider);
  service()->CheckManagementPolicy();
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  {
    auto function = base::MakeRefCounted<ManagementGetFunction>();
    std::optional<base::Value> value =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), args,
                                                         profile());
    ASSERT_TRUE(value);
    std::optional<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_FALSE(info->enabled);
    ASSERT_TRUE(info->may_enable);
    EXPECT_FALSE(*(info->may_enable));
  }

  // Re-enable the extension.
  policy->UnregisterAllProviders();
  service()->CheckManagementPolicy();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // Disable the extension with a normal user action. Verify the extension shows
  // as disabled with |may_enable| as true.
  registrar()->DisableExtension(extension->id(),
                                {disable_reason::DISABLE_USER_ACTION});
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  {
    auto function = base::MakeRefCounted<ManagementGetFunction>();
    std::optional<base::Value> value =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), args,
                                                         profile());
    ASSERT_TRUE(value);
    std::optional<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_FALSE(info->enabled);
    ASSERT_TRUE(info->may_enable);
    EXPECT_TRUE(*(info->may_enable));
  }
}

TEST_F(ManagementApiUnitTest, ExtensionInfo_MayDisable) {
  using ExtensionInfo = api::management::ExtensionInfo;

  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  registrar()->AddExtension(extension.get());

  const std::string args =
      base::StringPrintf("[\"%s\"]", extension->id().c_str());

  // Initially the extension should show as enabled, so it may be disabled
  // freely.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  {
    auto function = base::MakeRefCounted<ManagementGetFunction>();
    std::optional<base::Value> value =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), args,
                                                         profile());
    ASSERT_TRUE(value);
    std::optional<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->enabled);
    EXPECT_TRUE(info->may_disable);
  }

  // Simulate forcing the extension and verify that the extension shows with
  // a false value of |may_disable|.
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::MUST_REMAIN_ENABLED);
  policy->RegisterProvider(&provider);
  service()->CheckManagementPolicy();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  {
    auto function = base::MakeRefCounted<ManagementGetFunction>();
    std::optional<base::Value> value =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), args,
                                                         profile());
    ASSERT_TRUE(value);
    std::optional<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->enabled);
    EXPECT_FALSE(info->may_disable);
  }
}

TEST_F(ManagementApiUnitTest, SetEnabled_UnsupportedRequirements) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  // Install an extension with unsupported requirements.
  base::FilePath base_path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path = base_path.AppendASCII("v1_good.pem");
  base::FilePath path = base_path.AppendASCII("v2_bad_requirements");
  // No WebGL will be the unsupported requirement.
  content::GpuDataManager::GetInstance()->BlocklistWebGLForTesting();
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);

  std::string error;
  bool success = RunSetEnabledFunction(web_contents.get(), extension->id(),
                                       false /* use_user_gesture */,
                                       true /* accept_dialog */, &error);
  EXPECT_FALSE(success);
  EXPECT_EQ(error, "There were missing requirements: WebGL is not supported.");
  EXPECT_FALSE(registry()->enabled_extensions().Contains(extension->id()));
}

// Tests enabling an extension via management API after it was disabled due to
// permission increase.
TEST_F(ManagementApiUnitTest, SetEnabled_IncreasedPermissions) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);
  // The extension must now be installed and enabled.
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // Save the id, as |extension| will be destroyed during updating.
  ExtensionId extension_id = extension->id();

  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  // v1 extension doesn't have any permissions.
  EXPECT_TRUE(known_perms->IsEmpty());

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);

  // The extension should be disabled.
  ASSERT_FALSE(registry()->enabled_extensions().Contains(extension_id));

  // Due to a permission increase, prefs will contain escalation information.
  EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // 1) Confirm re-enable prompt without user gesture, expect the extension to
  // stay disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         false /* use_user_gesture */,
                                         true /* accept_dialog */, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 2) Deny re-enable prompt without user gesture, expect the extension to stay
  // disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         false /* use_user_gesture */,
                                         false /* accept_dialog */, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 3) Deny re-enable prompt with user gesture, expect the extension to stay
  // disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         true /* use_user_gesture */,
                                         false /* accept_dialog */, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 4) Accept re-enable prompt with user gesture, expect the extension to be
  // enabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         true /* use_user_gesture */,
                                         true /* accept_dialog */, &error);
    EXPECT_TRUE(success) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs will no longer contain the escalation information as user has
    // accepted increased permissions.
    EXPECT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // Some permissions for v2 extension should be granted by now.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_FALSE(known_perms->IsEmpty());
}

TEST_F(ManagementApiUnitTest,
       SetEnabled_UnsupportedRequirementsAndPermissionsIncrease) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  // Install an extension with unsupported requirements and permissions
  // increase.
  base::FilePath base_path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path = base_path.AppendASCII("v1_good.pem");
  base::FilePath path =
      base_path.AppendASCII("v2_bad_requirements_and_permissions");
  // No WebGL will be the unsupported requirement.
  content::GpuDataManager::GetInstance()->BlocklistWebGLForTesting();
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);

  // Unsupported requirements should fail first.
  std::string error;
  bool success = RunSetEnabledFunction(web_contents.get(), extension->id(),
                                       false /* use_user_gesture */,
                                       true /* accept_dialog */, &error);
  EXPECT_FALSE(success);
  EXPECT_EQ(error, "There were missing requirements: WebGL is not supported.");
  EXPECT_FALSE(registry()->enabled_extensions().Contains(extension->id()));
}

// A delegate that senses when extensions are enabled or disabled.
class TestManagementAPIDelegate : public ManagementAPIDelegate {
 public:
  TestManagementAPIDelegate() = default;
  ~TestManagementAPIDelegate() override = default;

  bool LaunchAppFunctionDelegate(
      const Extension* extension,
      content::BrowserContext* context) const override {
    return false;
  }
  GURL GetFullLaunchURL(const Extension* extension) const override {
    return GURL();
  }
  LaunchType GetLaunchType(const ExtensionPrefs* prefs,
                           const Extension* extension) const override {
    return LaunchType::kDefault;
  }
  std::unique_ptr<InstallPromptDelegate> SetEnabledFunctionDelegate(
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const Extension* extension,
      base::OnceCallback<void(bool)> callback) const override {
    return nullptr;
  }
  void EnableExtension(content::BrowserContext* context,
                       const ExtensionId& extension_id) const override {
    ++enable_count_;
  }
  void DisableExtension(
      content::BrowserContext* context,
      const Extension* source_extension,
      const ExtensionId& extension_id,
      disable_reason::DisableReason disable_reason) const override {}
  std::unique_ptr<UninstallDialogDelegate> UninstallFunctionDelegate(
      ManagementUninstallFunctionBase* function,
      const Extension* target_extension,
      bool show_programmatic_uninstall_ui) const override {
    return nullptr;
  }
  bool UninstallExtension(content::BrowserContext* context,
                          const ExtensionId& transient_extension_id,
                          UninstallReason reason,
                          std::u16string* error) const override {
    return true;
  }
  bool CreateAppShortcutFunctionDelegate(
      ManagementCreateAppShortcutFunction* function,
      const Extension* extension,
      std::string* error) const override {
    return true;
  }
  void SetLaunchType(content::BrowserContext* context,
                     const ExtensionId& extension_id,
                     LaunchType launch_type) const override {}

  GURL GetIconURL(const Extension* extension,
                  int icon_size,
                  ExtensionIconSet::Match match,
                  bool grayscale) const override {
    return GURL();
  }
  GURL GetEffectiveUpdateURL(const extensions::Extension& extension,
                             content::BrowserContext* context) const override {
    return GURL();
  }

  // EnableExtension is const, so this is mutable.
  mutable int enable_count_ = 0;
};

}  // namespace
}  // namespace extensions
