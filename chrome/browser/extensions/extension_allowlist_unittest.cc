// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_allowlist.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/crx_installer.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_blocklist.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Extension ids used during testing.
constexpr char kExtensionId1[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
constexpr char kExtensionId2[] = "hpiknbiabeeppbpihjehijgoemciehgk";
constexpr char kInstalledCrx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";

using ManagementPrefUpdater = ExtensionManagementPrefUpdater<
    sync_preferences::TestingPrefServiceSyncable>;

}  // namespace

// Test suite to test safe browsing allowlist enforcement.
//
// Features EnforceSafeBrowsingExtensionAllowlist and
// DisableMalwareExtensionsRemotely are enabled.
class ExtensionAllowlistUnitTestBase : public ExtensionServiceTestBase {
 protected:
  void TearDown() override {
    extension_prefs_ = nullptr;
    ExtensionServiceTestBase::TearDown();
  }

  // Creates a test extension service with 3 installed extensions.
  void CreateExtensionService(bool enhanced_protection_enabled) {
    ExtensionServiceInitParams params;
    ASSERT_TRUE(
        params.ConfigureByTestDataDirectory(data_dir().AppendASCII("good")));
    InitializeExtensionService(std::move(params));
    extension_prefs_ = ExtensionPrefs::Get(profile());

    if (enhanced_protection_enabled) {
      safe_browsing::SetSafeBrowsingState(
          profile()->GetPrefs(),
          safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
    }
  }

  void CreateEmptyExtensionService() {
    InitializeExtensionService(ExtensionServiceInitParams());
    extension_prefs_ = ExtensionPrefs::Get(profile());
    safe_browsing::SetSafeBrowsingState(
        profile()->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  }

  void PerformActionBasedOnOmahaAttributes(const ExtensionId& extension_id,
                                           bool is_malware,
                                           bool is_allowlisted) {
    auto attributes = base::DictValue().Set("_esbAllowlist", is_allowlisted);
    if (is_malware) {
      attributes.Set("_malware", true);
    }

    service()->PerformActionBasedOnOmahaAttributes(extension_id, attributes);
  }

  bool IsEnabled(const ExtensionId& extension_id) {
    return registry()->enabled_extensions().Contains(extension_id);
  }

  bool IsDisabled(const ExtensionId& extension_id) {
    return registry()->disabled_extensions().Contains(extension_id);
  }

  bool IsBlocklisted(const ExtensionId& extension_id) {
    return registry()->blocklisted_extensions().Contains(extension_id);
  }

  ExtensionAllowlist* allowlist() { return service()->allowlist(); }

  ExtensionPrefs* extension_prefs() { return extension_prefs_; }

 private:
  raw_ptr<ExtensionPrefs> extension_prefs_ = nullptr;
};

class ExtensionAllowlistUnitTest : public ExtensionAllowlistUnitTestBase {
 public:
  ExtensionAllowlistUnitTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kSafeBrowsingCrxAllowlistAutoDisable);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ExtensionAllowlistUnitTest, ReenabledExtensionsAreNotReenforced) {
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  // Start with a not allowlisted extension that was re-enabled by user.
  allowlist()->SetExtensionAllowlistState(kExtensionId1,
                                          ALLOWLIST_NOT_ALLOWLISTED);
  allowlist()->SetExtensionAllowlistAcknowledgeState(
      kExtensionId1, ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER);

  // And an extension that became allowlisted after it was re-enabled by user.
  allowlist()->SetExtensionAllowlistState(kExtensionId2, ALLOWLIST_ALLOWLISTED);
  allowlist()->SetExtensionAllowlistAcknowledgeState(
      kExtensionId2, ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER);

  service()->Init();
  // Even though ExtensionId1 is not allowlisted, it should stay enabled because
  // it was re-enabled by user.
  EXPECT_TRUE(IsEnabled(kExtensionId1));
  // Assert that ExtensionId2 is enabled before testing the allowlist state
  // change.
  EXPECT_TRUE(IsEnabled(kExtensionId2));

  // If `kExtensionId2` becomes not allowlisted again, it should stay enabled
  // because the user already chose to re-enable it in the past.
  PerformActionBasedOnOmahaAttributes(kExtensionId2,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_TRUE(IsEnabled(kExtensionId2));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId2));
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
}

TEST_F(ExtensionAllowlistUnitTest, BypassFrictionSetAckowledgeEnabledByUser) {
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(profile()));
  installer->set_allow_silent_install(true);
  installer->set_bypassed_safebrowsing_friction_for_testing(true);

  base::RunLoop run_loop;
  installer->AddInstallerCallback(base::BindOnce(
      [](base::OnceClosure quit_closure,
         const std::optional<CrxInstallError>& error) {
        ASSERT_FALSE(error) << error->message();
        std::move(quit_closure).Run();
      },
      run_loop.QuitWhenIdleClosure()));

  installer->InstallCrx(data_dir().AppendASCII("good.crx"));
  run_loop.Run();

  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kInstalledCrx));
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kInstalledCrx));
  EXPECT_EQ(ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER,
            allowlist()->GetExtensionAllowlistAcknowledgeState(kInstalledCrx));
}

TEST_F(ExtensionAllowlistUnitTest, NoEnforcementOnPolicyForceInstall) {
  // Mark as enterprise managed.
  policy::ScopedDomainEnterpriseManagement scoped_domain;
  CreateEmptyExtensionService();
  service()->Init();

  // Add a policy installed extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("policy_installed")
          .SetPath(data_dir().AppendASCII("good.crx"))
          .SetLocation(mojom::ManifestLocation::kExternalPolicyDownload)
          .Build();
  registrar()->AddExtension(extension.get());

  {
    ManagementPrefUpdater pref(testing_profile()->GetTestingPrefService());
    pref.SetIndividualExtensionAutoInstalled(
        extension->id(), "http://example.com/update_url", true);
  }

  EXPECT_TRUE(IsEnabled(extension->id()));

  // On next update check, the extension is now marked as not allowlisted.
  PerformActionBasedOnOmahaAttributes(extension->id(),
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(extension->id()));
  // A policy installed extension is not disabled by allowlist enforcement.
  EXPECT_TRUE(IsEnabled(extension->id()));
  // No warnings are shown for policy installed extensions.
  EXPECT_FALSE(allowlist()->ShouldDisplayWarning(extension->id()));
}

class ExtensionAllowlistWithFeatureDisabledUnitTest
    : public ExtensionAllowlistUnitTestBase {
 public:
  ExtensionAllowlistWithFeatureDisabledUnitTest() {
    // Test with warnings enabled but auto disable disabled.
    feature_list_.InitAndDisableFeature(
        extensions_features::kSafeBrowsingCrxAllowlistAutoDisable);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ExtensionAllowlistWithFeatureDisabledUnitTest,
       NoEnforcementWhenFeatureDisabled) {
  // Created with 3 installed extensions.
  CreateExtensionService(/*enhanced_protection_enabled=*/true);

  allowlist()->SetExtensionAllowlistState(kExtensionId1,
                                          ALLOWLIST_NOT_ALLOWLISTED);
  service()->Init();
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  PerformActionBasedOnOmahaAttributes(kExtensionId2,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_TRUE(IsEnabled(kExtensionId1));
}

// TODO(jeffcyr): Test with auto-disablement enabled when the enforcement is
// skipped for policy recommended and policy allowed extensions.
TEST_F(ExtensionAllowlistWithFeatureDisabledUnitTest,
       NoEnforcementOnPolicyRecommendedInstall) {
  CreateEmptyExtensionService();
  service()->Init();

  // Add a policy installed extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("policy_installed")
          .SetPath(data_dir().AppendASCII("good.crx"))
          .SetLocation(mojom::ManifestLocation::kExternalPrefDownload)
          .Build();
  registrar()->AddExtension(extension.get());

  {
    ManagementPrefUpdater pref(testing_profile()->GetTestingPrefService());
    pref.SetIndividualExtensionAutoInstalled(
        extension->id(), "http://example.com/update_url", false);
  }

  EXPECT_TRUE(IsEnabled(extension->id()));

  // On next update check, the extension is now marked as not allowlisted.
  PerformActionBasedOnOmahaAttributes(extension->id(),
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(extension->id()));
  // A policy installed extension is not disabled by allowlist enforcement.
  EXPECT_TRUE(IsEnabled(extension->id()));
  // No warnings are shown for policy installed extensions.
  EXPECT_FALSE(allowlist()->ShouldDisplayWarning(extension->id()));
}

TEST_F(ExtensionAllowlistWithFeatureDisabledUnitTest,
       NoEnforcementOnPolicyAllowedInstall) {
  CreateEmptyExtensionService();
  service()->Init();

  // Add a policy allowed extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("policy_allowed")
          .SetPath(data_dir().AppendASCII("good.crx"))
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  registrar()->AddExtension(extension.get());

  {
    ManagementPrefUpdater pref(testing_profile()->GetTestingPrefService());
    pref.SetIndividualExtensionInstallationAllowed(extension->id(), true);
  }

  EXPECT_TRUE(IsEnabled(extension->id()));

  // On next update check, the extension is now marked as not allowlisted.
  PerformActionBasedOnOmahaAttributes(extension->id(),
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(extension->id()));
  // An extension allowed by policy is not disabled by allowlist enforcement.
  EXPECT_TRUE(IsEnabled(extension->id()));
  // No warnings are shown for policy allowed extensions.
  EXPECT_FALSE(allowlist()->ShouldDisplayWarning(extension->id()));
}

// TODO(crbug.com/40175473): Add more ExtensionAllowlist::Observer coverage

}  // namespace extensions
