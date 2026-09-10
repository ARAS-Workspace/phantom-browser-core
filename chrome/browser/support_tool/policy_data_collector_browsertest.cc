// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/policy_data_collector.h"

#include <cstdio>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/policy_value_and_status_aggregator.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/browser/webui/policy_webui_constants.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::IsSubsetOf;

namespace {

// Reads the contents of exported policy Json file in to `policies`
// dictionary.
void ReadExportedPolicyFile(base::DictValue* policies,
                            base::FilePath file_path) {
  // Allow blocking for testing in this scope for IO operations.
  base::ScopedAllowBlockingForTesting allow_blocking;
  // `data_collector` will export the output into a file names
  // "policies.json" under `output_path`.
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(file_path, &file_contents));
  std::optional<base::Value> dict_value = base::JSONReader::Read(
      file_contents, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict_value);
  *policies = std::move(dict_value->GetDict());
}

class PolicyDataCollectorBrowserTest : public InProcessBrowserTest {
 public:
  PolicyDataCollectorBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    policy::PushProfilePolicyConnectorProviderForTesting(&policy_provider_);

    // Allow blocking for testing in this scope for temporary directory
    // creation.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDownInProcessBrowserTestFixture() override {
    // Allow blocking for testing in this scope for temporary directory
    // creation.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  void AddExpectedChromePolicy(policy::PolicyMap* policy_map,
                               base::DictValue* expected_policies,
                               const std::string& policy,
                               policy::PolicyLevel level,
                               const std::string& level_str,
                               policy::PolicyScope scope,
                               const std::string& scope_str,
                               policy::PolicySource source,
                               const std::string& source_str,
                               const std::string& error,
                               base::Value value) {
    policy_map->Set(policy, level, scope, source, value.Clone(),
                    /*external_data_fetcher=*/nullptr);
    base::DictValue policy_dict;
    policy_dict.Set("level", level_str);
    policy_dict.Set("scope", scope_str);
    policy_dict.Set("source", source_str);
    policy_dict.Set("value", std::move(value));
    if (!error.empty())
      policy_dict.Set("error", error);
    expected_policies->SetByDottedPath(
        base::StringPrintf("%s.%s", policy::kPoliciesKey, policy.c_str()),
        std::move(policy_dict));
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  // Use a temporary directory to store data collector output.
  base::ScopedTempDir temp_dir_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PolicyDataCollectorBrowserTest,
                       CollectPolicyValuesAndMetadata) {
  // PolicyDataCollector for testing.
  PolicyDataCollector data_collector(browser()->GetProfile());

  // We will use `values` for mocking the policy values `policy_provider_` will
  // return.
  policy::PolicyMap values;

  // Set expected policy values.
  base::DictValue expected_policies;
  expected_policies.Set(policy::kNameKey, policy::kChromePoliciesName);
  expected_policies.Set(policy::kPoliciesKey, base::DictValue());

  // Add policies for testing.
  base::ListValue popups_blocked_for_urls;
  popups_blocked_for_urls.Append("aaa");
  popups_blocked_for_urls.Append("bbb");
  popups_blocked_for_urls.Append("ccc");
  AddExpectedChromePolicy(
      &values, &expected_policies, policy::key::kPopupsBlockedForUrls,
      policy::POLICY_LEVEL_MANDATORY, "mandatory", policy::POLICY_SCOPE_MACHINE,
      "machine", policy::POLICY_SOURCE_PLATFORM, "platform",
      /*error=*/std::string(), base::Value(std::move(popups_blocked_for_urls)));

  AddExpectedChromePolicy(
      &values, &expected_policies, policy::key::kDefaultImagesSetting,
      policy::POLICY_LEVEL_MANDATORY, "mandatory", policy::POLICY_SCOPE_MACHINE,
      "machine", policy::POLICY_SOURCE_CLOUD, "cloud",
      /*error=*/std::string(), base::Value(2));

  // This also checks that we save unknown policies correctly.
  base::ListValue unknown_policy;
  unknown_policy.Append(true);
  unknown_policy.Append(12);
  const std::string kUnknownPolicy = "NoSuchThing";
  AddExpectedChromePolicy(
      &values, &expected_policies, kUnknownPolicy,
      policy::POLICY_LEVEL_RECOMMENDED, "recommended",
      policy::POLICY_SCOPE_USER, "user", policy::POLICY_SOURCE_CLOUD, "cloud",
      /*error=*/l10n_util::GetStringUTF8(IDS_POLICY_UNKNOWN),
      base::Value(std::move(unknown_policy)));

  // Add the Chrome policies to fake `policy_provider_`.
  policy_provider_.UpdateChromePolicy(values);

  // Collect policies and assert no error returned.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(
      test_future_collect_data.GetCallback(),
      /*task_runner_for_redaction_tool=*/nullptr,
      /*redaction_tool_container=*/nullptr);
  std::optional<SupportToolError> error = test_future_collect_data.Get();
  EXPECT_EQ(error, std::nullopt);

  // Create a temporary directory to store the output file.
  base::FilePath output_path = temp_dir_.GetPath();
  // Export the collected data into `output_path` and make sure no error is
  // returned.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_export_data;
  data_collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_path,
      /*task_runner_for_redaction_tool=*/nullptr,
      /*redaction_tool_container=*/nullptr,
      test_future_export_data.GetCallback());
  error = test_future_export_data.Get();
  EXPECT_EQ(error, std::nullopt);

  // The result must contain three main parts: "chromeMetadata", policies and
  // "status".
  base::DictValue policy_result;
  ASSERT_NO_FATAL_FAILURE(ReadExportedPolicyFile(
      &policy_result, output_path.Append(FILE_PATH_LITERAL("policies.json"))));

  base::DictValue* chrome_metadata = policy_result.FindDict("chromeMetadata");
  ASSERT_TRUE(chrome_metadata);
  // Check that `chrome_metadata` contains all the expected keys.
  // The keys that are expected to be in "chromeMetadata" dictionary are
  // "application", "version", "revision". We don't include platform specific
  // keys because we will test for all platforms.
  EXPECT_TRUE(chrome_metadata->contains("application"));
  EXPECT_TRUE(chrome_metadata->contains("version"));
  EXPECT_TRUE(chrome_metadata->contains("revision"));

  // Check that policy values are the same as expected.
  base::DictValue* policy_values =
      policy_result.FindDict(policy::kPolicyValuesKey);
  ASSERT_TRUE(policy_values);
  // We only check Chrome policies as it's common between all platforms. We
  // don't test platform specific policies in this test.
  base::DictValue* chrome_policies =
      policy_values->FindDict(policy::kChromePoliciesId);
  ASSERT_TRUE(chrome_policies);
  EXPECT_EQ(*chrome_policies, expected_policies);

  // Check that the returned contains "status". We just check if the returned
  // status is not empty.
  base::DictValue* status = policy_result.FindDict("status");
  ASSERT_TRUE(status);
  EXPECT_FALSE(status->empty());
}
