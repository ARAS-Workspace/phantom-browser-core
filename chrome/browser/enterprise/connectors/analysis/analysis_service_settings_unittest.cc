// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/analysis_service_settings.h"

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/analysis_test_utils.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

using test::GetExpectedLearnMoreUrlSpecs;
using test::kEnablePatternIsNotADictSettings;
using test::kNoEnabledPatternsSettings;
using test::kNoProviderSettings;
using test::kNormalSettings;
using test::kNormalSettingsDlpRequiresBypassJustification;
using test::kNormalSettingsWithCustomMessage;
using test::kScan1DotCom;
using test::kUrlAndSourceDestinationListSettings;
using test::NormalDlpAndMalwareSettings;
using test::NormalDlpSettings;
using test::NormalMalwareSettings;
using test::NormalSettingsDlpRequiresBypassJustification;
using test::NormalSettingsWithCustomMessage;
using test::NoSettings;
using test::OnlyDlpEnabledSettings;
using test::TestParam;

}  // namespace

class AnalysisServiceSettingsLocalTest
    : public testing::TestWithParam<TestParam> {
 public:
  GURL url() const { return GURL(GetParam().url); }
  std::string GetSettingsValue() const {
    static const char* verification = R"(
      "verification": {
        "linux": ["key"],
        "mac": ["key"],
        "windows": ["key"]
      },
    )";

    std::string value = GetParam().settings_value;
    base::ReplaceFirstSubstringAfterOffset(&value, 0, "%s", "local_user_agent");
    base::ReplaceFirstSubstringAfterOffset(&value, 0, "%s", verification);
    return value;
  }
  AnalysisSettings* expected_settings() const {
    // Set the GURL field dynamically to avoid static initialization issues.
    if (GetParam().expected_settings != NoSettings()) {
      LocalAnalysisSettings local_settings;
      local_settings.local_path = "path_user";
      local_settings.user_specific = true;
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
      local_settings.verification_signatures.push_back("key");
#endif
      GetParam().expected_settings->cloud_or_local_settings =
          CloudOrLocalAnalysisSettings(std::move(local_settings));

      // The "local_user_agent" analysis provider only supports the "dlp" tag,
      // so it is expected that the malware tag is absent from final settings
      // even when it is included in the policy.
      GetParam().expected_settings->tags.erase(kMalwareTag);
      if (GetParam().expected_settings->tags.empty()) {
        return NoSettings();
      }
    }

    return GetParam().expected_settings;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(AnalysisServiceSettingsLocalTest, LocalTest) {
  std::string json_string = GetSettingsValue();
  auto settings =
      base::JSONReader::Read(json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());

  AnalysisServiceSettings service_settings(settings.value(),
                                           *GetServiceProviderConfig());

  auto analysis_settings =
      service_settings.GetAnalysisSettings(url(), DataRegion::NO_PREFERENCE);
  ASSERT_EQ((expected_settings() != nullptr), analysis_settings.has_value());
  if (analysis_settings.has_value()) {
    ASSERT_EQ(analysis_settings.value().block_until_verdict,
              expected_settings()->block_until_verdict);
    ASSERT_EQ(analysis_settings.value().default_action,
              expected_settings()->default_action);
    ASSERT_EQ(analysis_settings.value().block_password_protected_files,
              expected_settings()->block_password_protected_files);
    ASSERT_EQ(analysis_settings.value().block_large_files,
              expected_settings()->block_large_files);
    ASSERT_EQ(analysis_settings.value().minimum_data_size,
              expected_settings()->minimum_data_size);

    const auto& cloud_or_local_settings =
        analysis_settings.value().cloud_or_local_settings;
    ASSERT_TRUE(cloud_or_local_settings.is_local_analysis());
    ASSERT_EQ(cloud_or_local_settings.local_path(),
              expected_settings()->cloud_or_local_settings.local_path());
    ASSERT_EQ(cloud_or_local_settings.user_specific(),
              expected_settings()->cloud_or_local_settings.user_specific());
    ASSERT_EQ(
        cloud_or_local_settings.verification_signatures(),
        expected_settings()->cloud_or_local_settings.verification_signatures());

    for (const auto& entry : expected_settings()->tags) {
      const std::string& tag = entry.first;
      ASSERT_TRUE(analysis_settings.value().tags.count(entry.first));
      ASSERT_EQ(analysis_settings.value().tags[tag].custom_message.message,
                entry.second.custom_message.message);
      if (!analysis_settings.value()
               .tags[tag]
               .custom_message.learn_more_url.is_empty()) {
        ASSERT_EQ(GetExpectedLearnMoreUrlSpecs().at(tag),
                  analysis_settings.value()
                      .tags[tag]
                      .custom_message.learn_more_url.spec());
        ASSERT_EQ(GetExpectedLearnMoreUrlSpecs().at(tag),
                  service_settings.GetLearnMoreUrl(tag).value().spec());
      }
      ASSERT_EQ(analysis_settings.value().tags[tag].requires_justification,
                entry.second.requires_justification);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AnalysisServiceSettingsLocalTest,
    testing::Values(
        // Validate that no settings are returned for various invalid or empty
        // configurations.
        TestParam(kScan1DotCom, kEnablePatternIsNotADictSettings, NoSettings()),
        TestParam(kScan1DotCom,
                  kUrlAndSourceDestinationListSettings,
                  NoSettings()),
        TestParam(kScan1DotCom, kNoProviderSettings, NoSettings()),
        TestParam(kScan1DotCom, kNoEnabledPatternsSettings, NoSettings()),

        // Validate local analysis settings, custom messages and bypass
        // justifications.
        TestParam(kScan1DotCom, kNormalSettings, NormalDlpAndMalwareSettings()),
        TestParam(kScan1DotCom,
                  kNormalSettingsWithCustomMessage,
                  NormalSettingsWithCustomMessage()),
        TestParam(kScan1DotCom,
                  kNormalSettingsDlpRequiresBypassJustification,
                  NormalSettingsDlpRequiresBypassJustification())));

}  // namespace enterprise_connectors
