// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_allowlist.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/extension_allowlist_factory.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Extension ids used during testing.
constexpr char kExtensionId1[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
constexpr char kExtensionId2[] = "hpiknbiabeeppbpihjehijgoemciehgk";

using ManagementPrefUpdater = ExtensionManagementPrefUpdater<
    sync_preferences::TestingPrefServiceSyncable>;

}  // namespace

// Test suite for the Safe Browsing allowlist state set from Omaha attributes.
class ExtensionAllowlistUnitTestBase : public ExtensionServiceTestBase {
 protected:
  // Creates a test extension service with 3 installed extensions.
  void CreateExtensionService() {
    ExtensionServiceInitParams params;
    ASSERT_TRUE(
        params.ConfigureByTestDataDirectory(data_dir().AppendASCII("good")));
    InitializeExtensionService(std::move(params));
  }

  void CreateEmptyExtensionService() {
    InitializeExtensionService(ExtensionServiceInitParams());
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

  ExtensionAllowlist* allowlist() {
    return ExtensionAllowlistFactory::GetForBrowserContext(profile());
  }
};

TEST_F(ExtensionAllowlistUnitTestBase, ReenabledExtensionsAreNotReenforced) {
  CreateExtensionService();

  // Start with a not allowlisted extension.
  allowlist()->SetExtensionAllowlistState(kExtensionId1,
                                          ALLOWLIST_NOT_ALLOWLISTED);

  // And an allowlisted extension.
  allowlist()->SetExtensionAllowlistState(kExtensionId2, ALLOWLIST_ALLOWLISTED);

  service()->Init();
  // Even though ExtensionId1 is not allowlisted, it should stay enabled.
  EXPECT_TRUE(IsEnabled(kExtensionId1));
  // Assert that ExtensionId2 is enabled before testing the allowlist state
  // change.
  EXPECT_TRUE(IsEnabled(kExtensionId2));

  // If `kExtensionId2` becomes not allowlisted, it should stay enabled.
  PerformActionBasedOnOmahaAttributes(kExtensionId2,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_TRUE(IsEnabled(kExtensionId2));
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));

  // If `kExtensionId2` becomes allowlisted again, the state follows the Omaha
  // attribute.
  PerformActionBasedOnOmahaAttributes(kExtensionId2,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/true);
  EXPECT_EQ(ALLOWLIST_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
}

TEST_F(ExtensionAllowlistUnitTestBase, NoEnforcementWhenFeatureDisabled) {
  // Created with 3 installed extensions.
  CreateExtensionService();

  allowlist()->SetExtensionAllowlistState(kExtensionId1,
                                          ALLOWLIST_NOT_ALLOWLISTED);
  service()->Init();
  EXPECT_TRUE(IsEnabled(kExtensionId1));

  PerformActionBasedOnOmahaAttributes(kExtensionId2,
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);
  EXPECT_TRUE(IsEnabled(kExtensionId1));
  EXPECT_TRUE(IsEnabled(kExtensionId2));
  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(kExtensionId2));
}

TEST_F(ExtensionAllowlistUnitTestBase,
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

  base::HistogramTester histogram_tester;
  // On next update check, the extension is now marked as not allowlisted.
  PerformActionBasedOnOmahaAttributes(extension->id(),
                                      /*is_malware=*/false,
                                      /*is_allowlisted=*/false);

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            allowlist()->GetExtensionAllowlistState(extension->id()));
  // 2 == ExtensionAllowlistOmahaAttributeValue::kNotAllowlisted.
  histogram_tester.ExpectUniqueSample("Extensions.EsbAllowlistOmahaAttribute",
                                      /*sample=*/2,
                                      /*expected_bucket_count=*/1);
  // A policy installed extension is not disabled by allowlist enforcement.
  EXPECT_TRUE(IsEnabled(extension->id()));
}

TEST_F(ExtensionAllowlistUnitTestBase, NoEnforcementOnPolicyAllowedInstall) {
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
}

}  // namespace extensions
