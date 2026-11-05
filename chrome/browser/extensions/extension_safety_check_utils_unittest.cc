// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_safety_check_utils.h"

#include "base/strings/string_util.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

const scoped_refptr<const Extension> CreateExtension(
    const std::string& name,
    base::ListValue permissions,
    mojom::ManifestLocation location,
    const std::string& update_url = extension_urls::kChromeWebstoreUpdateURL) {
  const ExtensionId kId = crx_file::id_util::GenerateId(name);
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::DictValue()
                           .Set("name", name)
                           .Set("description", "an extension")
                           .Set("manifest_version", 2)
                           .Set("version", "1.0.0")
                           .Set("permissions", std::move(permissions))
                           .Set("update_url", update_url))
          .SetLocation(location)
          .SetID(kId)
          .Build();

  return extension;
}

// Ensures that the warning_reason and the safety check strings match.
void CheckSafetyCheckDisplayString(
    api::developer_private::SafetyCheckWarningReason warning_reason,
    api::developer_private::SafetyCheckStrings strings,
    bool extension_state = true) {
  int detail_page_string = 0;
  int panel_string = 0;
  switch (warning_reason) {
    case api::developer_private::SafetyCheckWarningReason::kMalware:
      detail_page_string = IDS_SAFETY_CHECK_EXTENSIONS_MALWARE;
      panel_string = IDS_EXTENSIONS_SC_MALWARE;
      break;
    case api::developer_private::SafetyCheckWarningReason::kPolicy:
      detail_page_string = IDS_SAFETY_CHECK_EXTENSIONS_POLICY_VIOLATION;
      panel_string = extension_state ? IDS_EXTENSIONS_SC_POLICY_VIOLATION_ON
                                     : IDS_EXTENSIONS_SC_POLICY_VIOLATION_OFF;
      break;
    case api::developer_private::SafetyCheckWarningReason::kUnwanted:
      detail_page_string = IDS_SAFETY_CHECK_EXTENSIONS_POLICY_VIOLATION;
      panel_string = extension_state ? IDS_EXTENSIONS_SC_POLICY_VIOLATION_ON
                                     : IDS_EXTENSIONS_SC_POLICY_VIOLATION_OFF;
      break;
    case api::developer_private::SafetyCheckWarningReason::kNoPrivacyPractice:
    case api::developer_private::SafetyCheckWarningReason::kOffstore:
    case api::developer_private::SafetyCheckWarningReason::kUnpublished:
    case api::developer_private::SafetyCheckWarningReason::kNone:
      EXPECT_FALSE(strings.detail_string.has_value());
      EXPECT_FALSE(strings.panel_string.has_value());
      return;
  }
  EXPECT_EQ(strings.detail_string,
            l10n_util::GetStringUTF8(detail_page_string));
  EXPECT_EQ(strings.panel_string, l10n_util::GetStringUTF8(panel_string));
}

class SafetyCheckExtensionUtilsTest : public testing::Test {
 public:
  SafetyCheckExtensionUtilsTest() = default;
  SafetyCheckExtensionUtilsTest(const SafetyCheckExtensionUtilsTest&) = delete;
  SafetyCheckExtensionUtilsTest& operator=(
      const SafetyCheckExtensionUtilsTest&) = delete;
  ~SafetyCheckExtensionUtilsTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    feature_list_.InitAndEnableFeature(
        features::kSafetyHubExtensionsUwSTrigger);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_Malware) {
  // Test that a malware extension will trigger the Extension
  // Review Panel based on if it has been kept or not.
  const scoped_refptr<const Extension> extension = CreateExtension(
      "test", base::ListValue(), mojom::ManifestLocation::kInternal);
  {
    // Blocklist - Malware.
    api::developer_private::SafetyCheckWarningReason malware_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::BLOCKLISTED_MALWARE, profile_.get(),
            *extension);
    // A malware warning will be shown since the blocklist bit returns
    // malware.
    EXPECT_EQ(malware_warning,
              api::developer_private::SafetyCheckWarningReason::kMalware);
  }
  {
    // Blocklist - Malware - Kept for minor violation.
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Unpublished Warning Reason*/ 1);
    api::developer_private::SafetyCheckWarningReason malware_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::BLOCKLISTED_MALWARE, profile_.get(),
            *extension);
    // A malware warning will be shown since the previously acknowledged
    // warning has a lesser warning level.
    EXPECT_EQ(malware_warning,
              api::developer_private::SafetyCheckWarningReason::kMalware);
  }
  {
    // Blocklist - Malware - Kept for Malware.
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Malware Warning Reason*/ 3);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::BLOCKLISTED_MALWARE, profile_.get(),
            *extension);
    // No warning will be shown since the previously acknowledged
    // warning was malware.
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_PrefMigration) {
  // Test that the `PrefAcknowledgeSafetyCheckWarningReason` migration
  // works as intended
  const scoped_refptr<const Extension> extension = CreateExtension(
      "test", base::ListValue(), mojom::ManifestLocation::kInternal);
  {
    // Verify that the boolean `kPrefAcknowledgeSafetyCheckWarning` is
    // deleted if the `kPrefAcknowledgeSafetyCheckWarningReason` warning
    // reason pref is already present, and that the
    // `kPrefAcknowledgeSafetyCheckWarningReason` is unchanged.
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Policy Warning Reason*/ 2);
    ExtensionPrefs::Get(profile_.get())
        ->SetBooleanPref(extension->id(), kPrefAcknowledgeSafetyCheckWarning,
                         true);
    ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
        BitMapBlocklistState::BLOCKLISTED_MALWARE, profile_.get(), *extension);
    bool warning = false;
    int warning_reason = 0;
    EXPECT_FALSE(
        ExtensionPrefs::Get(profile_.get())
            ->ReadPrefAsBoolean(extension->id(),
                                extensions::kPrefAcknowledgeSafetyCheckWarning,
                                &warning));
    EXPECT_TRUE(ExtensionPrefs::Get(profile_.get())
                    ->ReadPrefAsInteger(
                        extension->id(),
                        extensions::kPrefAcknowledgeSafetyCheckWarningReason,
                        &warning_reason));
    EXPECT_EQ(warning_reason, /*Policy Warning Reason*/ 2);
  }
  {
    // Verify that if only the bool `kPrefAcknowledgeSafetyCheckWarning`
    // is present, then:
    //   - The boolean pref is deleted
    //   - The `kPrefAcknowledgeSafetyCheckWarningReason` pref is added with
    //     the current warning reason
    ExtensionPrefs::Get(profile_.get())
        ->SetBooleanPref(extension->id(), kPrefAcknowledgeSafetyCheckWarning,
                         true);
    ExtensionPrefs::Get(profile_.get())
        ->UpdateExtensionPref(
            extension->id(),
            extensions::kPrefAcknowledgeSafetyCheckWarningReason.name,
            std::nullopt);
    ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
        BitMapBlocklistState::BLOCKLISTED_MALWARE, profile_.get(), *extension);
    bool warning = false;
    EXPECT_FALSE(
        ExtensionPrefs::Get(profile_.get())
            ->ReadPrefAsBoolean(extension->id(),
                                extensions::kPrefAcknowledgeSafetyCheckWarning,
                                &warning));
    int warning_reason = 0;
    EXPECT_TRUE(ExtensionPrefs::Get(profile_.get())
                    ->ReadPrefAsInteger(
                        extension->id(),
                        extensions::kPrefAcknowledgeSafetyCheckWarningReason,
                        &warning_reason));
    EXPECT_EQ(warning_reason, /*Malware Warning Reason*/ 3);
  }
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_OffStore) {
  // Test that an extension that does not come from the store does not trigger
  // the Extension Review Panel.
  {
    // Unpacked extension outside of developer mode.
    const scoped_refptr<const Extension> extension_unpacked = CreateExtension(
        "test2", base::ListValue(), mojom::ManifestLocation::kUnpacked);
    profile_.get()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode,
                                           false);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::NOT_BLOCKLISTED, profile_.get(),
            *extension_unpacked);
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
  {
    // Packed extension with a third party update URL.
    const scoped_refptr<const Extension> extension_not_webstore =
        CreateExtension("test", base::ListValue(),
                        mojom::ManifestLocation::kInternal,
                        "https://example.com");
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::NOT_BLOCKLISTED, profile_.get(),
            *extension_not_webstore);
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_Policy) {
  // Test that a extension with a policy violation will trigger
  // the Extension Review Panel based on if it has been kept or not.
  const scoped_refptr<const Extension> extension = CreateExtension(
      "test", base::ListValue(), mojom::ManifestLocation::kInternal);
  {
    // Blocklist - Policy.
    api::developer_private::SafetyCheckWarningReason policy_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension);
    // A policy warning will be shown since the previously acknowledged
    // warning has a lesser warning level.
    EXPECT_EQ(policy_warning,
              api::developer_private::SafetyCheckWarningReason::kPolicy);
  }
  {
    // Blocklist - Policy - Kept for minor violation.
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Unpublished Warning Reason*/ 1);
    api::developer_private::SafetyCheckWarningReason policy_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension);
    // A policy warning will be shown since the previously acknowledged
    // warning has a lesser warning level.
    EXPECT_EQ(policy_warning,
              api::developer_private::SafetyCheckWarningReason::kPolicy);
  }
  {
    // Blocklist - Policy - Kept for Malware.
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Malware Warning Reason*/ 3);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension);
    // No warning will be shown since the previously acknowledged
    // warning has a higher warning level.
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_PotentiallyUnwanted) {
  // Test that a potentially unwanted extension will trigger the Extension
  // Review Panel based on if it has been kept or not.
  const scoped_refptr<const Extension> extension = CreateExtension(
      "test", base::ListValue(), mojom::ManifestLocation::kInternal);
  // Blocklist - Potentially unwanted.
  api::developer_private::SafetyCheckWarningReason unwanted =
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
          BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
          profile_.get(), *extension);
  // A warning will be shown since the extension has an unwanted blocklist
  // bit flipped.
  EXPECT_EQ(unwanted,
            api::developer_private::SafetyCheckWarningReason::kUnwanted);
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_No_Warning) {
  {
    // Test that no warning is shown for an extension without any store
    // violations.
    const scoped_refptr<const Extension> extension = CreateExtension(
        "test", base::ListValue(), mojom::ManifestLocation::kInternal);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::NOT_BLOCKLISTED, profile_.get(), *extension);
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
  {
    // Test that no warning is shown for a component extension.
    const scoped_refptr<const Extension> extension_component_location =
        CreateExtension("test", base::ListValue(),
                        mojom::ManifestLocation::kComponent);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension_component_location);
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
  {
    // Test that an extension explicitly allowed by policy does not
    // trigger a review panel warning.
    using PolicyUpdater = extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>;
    const scoped_refptr<const Extension> extension_policy_location =
        CreateExtension("test", base::ListValue(),
                        mojom::ManifestLocation::kInternal);
    sync_preferences::TestingPrefServiceSyncable* prefs =
        profile_->GetTestingPrefService();
    PolicyUpdater(prefs).SetIndividualExtensionInstallationAllowed(
        extension_policy_location->id(), true);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension_policy_location);
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
  {
    // Test than an extension force-installed by policy does not trigger a
    // review panel warning.
    using PolicyUpdater = extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>;
    const scoped_refptr<const Extension> extension_policy_location =
        CreateExtension("test", base::ListValue(),
                        mojom::ManifestLocation::kInternal);
    sync_preferences::TestingPrefServiceSyncable* prefs =
        profile_->GetTestingPrefService();
    PolicyUpdater(prefs).SetIndividualExtensionAutoInstalled(
        extension_policy_location->id(),
        extension_urls::kChromeWebstoreUpdateURL, true);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension_policy_location);
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_String_Check) {
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kUnpublished,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kUnpublished,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kMalware,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kMalware,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kPolicy,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kPolicy,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kOffstore,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kOffstore,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kUnpublished,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kUnpublished,
          api::developer_private::ExtensionState::kDisabled),
      false);
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kNoPrivacyPractice,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kNoPrivacyPractice,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kUnwanted,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kUnwanted,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kNone,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kNone,
          api::developer_private::ExtensionState::kEnabled));
}

}  // namespace
}  // namespace extensions
