// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui_handler.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/webui/management/management_ui_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/real_time_report_type.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "services/network/test/test_network_connection_tracker.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/device_signals/core/browser/mock_user_permission_service.h"  // nogncheck
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

using testing::_;
using testing::AnyNumber;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;

struct ContextualManagementSourceUpdate {
  std::u16string extension_reporting_subtitle;
  std::u16string managed_websites_title;
  std::u16string subtitle;
  std::u16string browser_management_notice;
  bool managed;
};

namespace {
const char kUser[] = "user@domain.com";
}  // namespace

using ManagementUIHandlerBase = ManagementUIHandler;
class TestManagementUIHandler : public ManagementUIHandlerBase {
 public:
  TestManagementUIHandler() : ManagementUIHandlerBase(/*profile=*/nullptr) {}
  TestManagementUIHandler(policy::PolicyService* policy_service,
                          content::WebUI* web_ui)
      : ManagementUIHandlerBase(/*profile=*/nullptr),
        policy_service_(policy_service) {
    set_web_ui(web_ui);
  }

  ~TestManagementUIHandler() override = default;

  void EnableUpdateRequiredEolInfo(bool enable) {
    update_required_eol_ = enable;
  }

  base::DictValue GetContextualManagedDataForTesting(Profile* profile) {
    return GetContextualManagedData(profile);
  }

  base::ListValue GetReportingInfo(bool can_collect_signals = true,
                                   bool is_browser = true) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    EXPECT_CALL(mock_user_permission_service_, CanCollectSignals())
        .WillOnce(
            Return((can_collect_signals)
                       ? device_signals::UserPermission::kGranted
                       : device_signals::UserPermission::kMissingConsent));
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    base::ListValue report_sources;
    if (is_browser) {
      AddBrowserReportingInfo(&report_sources);
    } else {
      AddProfileReportingInfo(&report_sources);
    }
    return report_sources;
  }

  base::ListValue GetManagedWebsitesInfo(Profile* profile) {
    return ManagementUIHandlerBase::GetManagedWebsitesInfo(profile);
  }

  base::DictValue GetThreatProtectionInfo(Profile* profile) {
    return ManagementUIHandlerBase::GetThreatProtectionInfo(profile);
  }

  policy::PolicyService* GetPolicyService() override { return policy_service_; }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  device_signals::UserPermissionService* GetUserPermissionService() override {
    return &mock_user_permission_service_;
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

 private:
  raw_ptr<policy::PolicyService> policy_service_ = nullptr;
  bool update_required_eol_ = false;
  std::string device_domain = "devicedomain.com";
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  device_signals::MockUserPermissionService mock_user_permission_service_;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
};

// We need to use a different base class for ChromeOS and non ChromeOS case.
// TODO(1071436, marcgrimme): refactor so that ChromeOS and non ChromeOS part is
// better separated.
class ManagementUIHandlerTests :
    public testing::Test
{
 public:
  ManagementUIHandlerTests() : handler_(&policy_service_, &web_ui_) {
    ON_CALL(policy_service_, GetPolicies(_))
        .WillByDefault(ReturnRef(empty_policy_map_));
  }

  ~ManagementUIHandlerTests() override = default;

  std::u16string device_domain() { return device_domain_; }
  void EnablePolicy(const char* policy_key, policy::PolicyMap& policies) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(true), nullptr);
  }
  void SetPolicyValue(const char* policy_key,
                      int value,
                      policy::PolicyMap& policies) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(value), nullptr);
  }
  void SetPolicyValue(const char* policy_key,
                      bool value,
                      policy::PolicyMap& policies) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(value), nullptr);
  }
  void SetConnectorPolicyValue(const char* policy_key,
                               const std::string& value,
                               policy::PolicyMap& policies) {
    auto policy_value =
        base::JSONReader::Read(value, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    EXPECT_TRUE(policy_value.has_value());
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::move(policy_value.value()), nullptr);
  }

  std::u16string ExtractPathFromDict(const base::DictValue& data,
                                     const std::string path) {
    const std::string* buf = data.FindStringByDottedPath(path);
    if (!buf) {
      return std::u16string();
    }
    return base::UTF8ToUTF16(*buf);
  }

  void ExtractContextualSourceUpdate(const base::DictValue& data) {
    extracted_.extension_reporting_subtitle =
        ExtractPathFromDict(data, "extensionReportingSubtitle");
    extracted_.managed_websites_title =
        ExtractPathFromDict(data, "managedWebsitesSubtitle");
    extracted_.subtitle = ExtractPathFromDict(data, "pageSubtitle");
    extracted_.browser_management_notice =
        ExtractPathFromDict(data, "browserManagementNotice");
    std::optional<bool> managed = data.FindBool("managed");
    extracted_.managed = managed.has_value() && managed.value();
  }

  /* Structure to organize the different configuration settings for each test
   * into configuration for a test case. */
  struct TestConfig {
    bool report_activity_times;
    bool report_nics;
    bool report_hardware_data;
    bool report_users;
    bool report_crash_info;
    bool report_app_info_and_activity;
    bool report_dlp_events;
    bool report_audio_status;
    bool report_device_peripherals;
    bool device_report_xdr_events;
    bool upload_enabled;
    bool printing_send_username_and_filename;
    bool crostini_report_usage;
    bool cloud_reporting_enabled;
    bool cloud_profile_reporting_enabled;
    std::string profile_name = kUser;
    bool override_policy_connector_is_managed;
    bool managed_account;
    bool managed_browser;
    bool managed_device;
    std::string device_domain;
    bool insights_extension_enabled;
    bool legacy_tech_reporting_enabled;
    bool real_time_url_check_connector_enabled;
    base::ListValue report_app_inventory;
    base::ListValue report_app_usage;
    base::ListValue report_website_telemetry;
    base::ListValue report_website_activity_allowlist;
    base::ListValue report_website_telemetry_allowlist;
    bool sync_windows;
    bool sync_cookies;
    bool saas_reporting_browser_enabled;
    bool saas_reporting_profile_enabled;
  };

  void ResetTestConfig() { ResetTestConfig(true); }

  void ResetTestConfig(bool default_value) {
    setup_config_.report_activity_times = default_value;
    setup_config_.report_nics = default_value;
    setup_config_.report_hardware_data = default_value;
    setup_config_.report_users = default_value;
    setup_config_.report_crash_info = default_value;
    setup_config_.report_app_info_and_activity = default_value;
    setup_config_.report_dlp_events = default_value;
    setup_config_.report_audio_status = default_value;
    setup_config_.report_device_peripherals = default_value;
    setup_config_.device_report_xdr_events = default_value;
    setup_config_.upload_enabled = default_value;
    setup_config_.printing_send_username_and_filename = default_value;
    setup_config_.crostini_report_usage = default_value;
    setup_config_.cloud_reporting_enabled = default_value;
    setup_config_.cloud_profile_reporting_enabled = default_value;
    setup_config_.profile_name = kUser;
    setup_config_.override_policy_connector_is_managed = false;
    setup_config_.managed_account = true;
    setup_config_.managed_browser = true;
    setup_config_.managed_device = false;
    setup_config_.device_domain = "devicedomain.com";
    setup_config_.insights_extension_enabled = false;
    setup_config_.report_app_inventory = base::ListValue();
    setup_config_.report_app_usage = base::ListValue();
    setup_config_.report_website_telemetry = base::ListValue();
    setup_config_.report_website_activity_allowlist = base::ListValue();
    setup_config_.report_website_telemetry_allowlist = base::ListValue();
    setup_config_.legacy_tech_reporting_enabled = false;
    setup_config_.real_time_url_check_connector_enabled = default_value;
    setup_config_.sync_windows = false;
    setup_config_.sync_cookies = false;
    setup_config_.saas_reporting_browser_enabled = false;
    setup_config_.saas_reporting_profile_enabled = false;
  }

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
  }
  void TearDown() override {
    web_contents_.reset();
    profile_ = nullptr;
    profile_manager_.reset();
  }

  [[nodiscard]] bool SetUpProfileAndHandler() {
    if (profile_) {
      return false;
    }

    profile_ =
        profile_manager_->CreateTestingProfile(GetTestConfig().profile_name);
    if (GetTestConfig().override_policy_connector_is_managed) {
      profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
    }

    if (GetTestConfig().saas_reporting_browser_enabled) {
      base::ListValue urls;
      urls.Append("https://example.com");
      TestingBrowserProcess::GetGlobal()->local_state()->SetList(
          enterprise_reporting::kSaasUsageDomainUrlsForBrowser,
          std::move(urls));
    }

    if (GetTestConfig().saas_reporting_profile_enabled) {
      base::ListValue urls;
      urls.Append("https://example.com");
      profile_->GetTestingPrefService()->SetManagedPref(
          enterprise_reporting::kSaasUsageDomainUrlsForProfile,
          std::make_unique<base::Value>(std::move(urls)));
    }

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());
    handler_.SetAccountManagedForTesting(GetTestConfig().managed_account);
    handler_.SetBrowserManagedForTesting(GetTestConfig().managed_browser);
    base::DictValue data =
        handler_.GetContextualManagedDataForTesting(profile_);
    ExtractContextualSourceUpdate(data);

    return true;
  }

  [[nodiscard]] bool SetUpNoDomainProfile() {
    if (profile_) {
      return false;
    }
    profile_ =
        profile_manager_->CreateTestingProfile(GetTestConfig().profile_name);
    return true;
  }

  bool GetManaged() const { return extracted_.managed; }

  std::u16string GetBrowserManagementNotice() const {
    return extracted_.browser_management_notice;
  }

  std::u16string GetExtensionReportingSubtitle() const {
    return extracted_.extension_reporting_subtitle;
  }

  std::u16string GetManagedWebsitesTitle() const {
    return extracted_.managed_websites_title;
  }

  std::u16string GetPageSubtitle() const { return extracted_.subtitle; }

  TestingProfile* GetProfile() const { return profile_; }

  TestConfig& GetTestConfig() { return setup_config_; }

 protected:
  TestConfig setup_config_;
  policy::MockPolicyService policy_service_;
  policy::PolicyMap empty_policy_map_;
  std::u16string device_domain_;
  ContextualManagementSourceUpdate extracted_;
  TestingPrefServiceSimple user_prefs_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;

  TestManagementUIHandler handler_;
};

AssertionResult MessagesToBeEQ(const char* infolist_expr,
                               const char* expected_infolist_expr,
                               const base::ListValue& infolist,
                               const std::set<std::string>& expected_messages) {
  std::set<std::string> tmp_expected(expected_messages);
  std::vector<std::string> tmp_info_messages;
  for (const base::Value& tmp_info : infolist) {
    const std::string* message = tmp_info.GetDict().FindString("messageId");
    if (message) {
      if (tmp_expected.erase(*message) != 1u) {
        tmp_info_messages.push_back(*message);
      }
    }
  }
  if (!tmp_expected.empty()) {
    AssertionResult result = AssertionFailure();
    result << "Expected messages from " << expected_infolist_expr
           << " has more contents than " << infolist_expr << std::endl
           << "Messages missing from test: ";
    for (const std::string& message : tmp_expected) {
      result << message << ", ";
    }
    return result;
  }
  if (!tmp_info_messages.empty()) {
    AssertionResult result = AssertionFailure();
    result << "Recieved messages from " << infolist_expr
           << " has more contents than " << expected_infolist_expr << std::endl
           << "Additional messages not expected: ";
    for (const std::string& message : tmp_info_messages) {
      result << message << ", ";
    }
    return result;
  }
  if (infolist.size() != expected_messages.size()) {
    return AssertionFailure()
           << " " << infolist_expr << " and " << expected_infolist_expr
           << " don't have the same size. (info: " << infolist.size()
           << ", expected: " << expected_messages.size() << ")";
  }
  return AssertionSuccess();
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateProfileManagedOnly) {
  ResetTestConfig();
  GetTestConfig().managed_browser = false;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_PROFILE_NOTICE, chrome::kManagedUiLearnMoreUrl,
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests, VerifyBasicAddReportingInfo) {
  ResetTestConfig();
  ASSERT_TRUE(SetUpProfileAndHandler());

  // 1. Test Browser Reporting Branch
  base::ListValue browser_reports = handler_.GetReportingInfo(
      /*can_collect_signals=*/true, /*is_browser=*/true);
  EXPECT_GE(browser_reports.size(), 0u);

  // 2. Test Profile Reporting Branch
  profile_->GetTestingPrefService()->SetManagedPref(
      enterprise_reporting::kCloudProfileReportingEnabled,
      std::make_unique<base::Value>(true));
  base::ListValue profile_reports = handler_.GetReportingInfo(
      /*can_collect_signals=*/true, /*is_browser=*/false);
  EXPECT_GE(profile_reports.size(), 0u);
}

TEST_F(ManagementUIHandlerTests, VerifyReportingTypeValues) {
  ResetTestConfig();
  ASSERT_TRUE(SetUpProfileAndHandler());
  // Verifies that GetReportingTypeValue logic is reachable and correct.
  // This covers the newly added ReportingType enum usage in the source file.
  base::ListValue reports = handler_.GetReportingInfo();
  for (const auto& report : reports) {
    const std::string* type = report.GetDict().FindString("reportingType");
    EXPECT_NE(type, nullptr);
  }
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateBrowserManagedOnly) {
  ResetTestConfig();
  GetTestConfig().managed_account = false;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE, chrome::kManagedUiLearnMoreUrl,
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedNoDomain) {
  ResetTestConfig();
  GetTestConfig().managed_account = false;
  GetTestConfig().managed_browser = false;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(
      GetBrowserManagementNotice(),
      l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_NOT_MANAGED_NOTICE, chrome::kManagedUiLearnMoreUrl,
          base::EscapeForHTML(l10n_util::GetStringUTF16(
              IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedNoDomain) {
  ResetTestConfig();
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE, chrome::kManagedUiLearnMoreUrl,
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedConsumerDomain) {
  ResetTestConfig();
  GetTestConfig().override_policy_connector_is_managed = true;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE, chrome::kManagedUiLearnMoreUrl,
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedKnownDomain) {
  const std::string domain = "manager.com";
  ResetTestConfig();
  GetTestConfig().profile_name = "managed@" + domain;
  GetTestConfig().override_policy_connector_is_managed = true;
  GetTestConfig().managed_account = false;
  GetTestConfig().managed_browser = false;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));

  EXPECT_EQ(
      GetBrowserManagementNotice(),
      l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_NOT_MANAGED_NOTICE, chrome::kManagedUiLearnMoreUrl,
          base::EscapeForHTML(l10n_util::GetStringUTF16(
              IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_FALSE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedCustomerDomain) {
  ResetTestConfig();
  GetTestConfig().managed_account = false;
  GetTestConfig().managed_browser = false;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(
      GetBrowserManagementNotice(),
      l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_NOT_MANAGED_NOTICE, chrome::kManagedUiLearnMoreUrl,
          base::EscapeForHTML(l10n_util::GetStringUTF16(
              IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
  EXPECT_FALSE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedKnownDomain) {
  const std::string domain = "gmail.com.manager.com.gmail.com";
  ResetTestConfig();
  GetTestConfig().profile_name = "managed@" + domain;
  GetTestConfig().override_policy_connector_is_managed = true;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));

  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE, chrome::kManagedUiLearnMoreUrl,
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_TRUE(GetManaged());
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
TEST_F(ManagementUIHandlerTests, ExtensionReportingInfoNoPolicySetNoMessage) {
  ResetTestConfig();
  ASSERT_TRUE(SetUpProfileAndHandler());
  auto reporting_info =
      handler_.GetReportingInfo(/*can_collect_signals=*/false);
  EXPECT_EQ(reporting_info.size(), 0u);
}
#endif

TEST_F(ManagementUIHandlerTests, CloudReportingPolicy) {
  ResetTestConfig();
  policy::PolicyMap policies;
  EXPECT_CALL(policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(policies));
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      enterprise_reporting::kCloudReportingEnabled, true);
  ASSERT_TRUE(SetUpProfileAndHandler());

  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, 1);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      kManagementExtensionReportExtensionsPlugin
#endif
  };
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)
  expected_messages.insert(kManagementExtensionReportVisitedUrl);
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  expected_messages.insert(kManagementDeviceSignalsDisclosure);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("fake-token"));
  ASSERT_PRED_FORMAT2(MessagesToBeEQ, handler_.GetReportingInfo(),
                      expected_messages);
}

TEST_F(ManagementUIHandlerTests, CloudProfileReportingPolicy) {
  ResetTestConfig(false);
  ASSERT_TRUE(SetUpProfileAndHandler());
  profile_->GetTestingPrefService()->SetManagedPref(
      enterprise_reporting::kCloudProfileReportingEnabled,
      std::make_unique<base::Value>(true));

  std::set<std::string> expected_messages = {
      kProfileReportingOverview, kProfileReportingUsername,
      kProfileReportingBrowser,
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      kProfileReportingExtension,
#endif
      kProfileReportingPolicy,   kProfileReportingLearnMore};

  ASSERT_PRED_FORMAT2(MessagesToBeEQ,
                      handler_.GetReportingInfo(/*can_collect_signals=*/false,
                                                /*is_browser=*/true),
                      {});
  ASSERT_PRED_FORMAT2(MessagesToBeEQ,
                      handler_.GetReportingInfo(/*can_collect_signals=*/false,
                                                /*is_browser=*/false),
                      expected_messages);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(ManagementUIHandlerTests,
       CloudReportingPolicyWithoutDeviceSignalsConsent) {
  ResetTestConfig();
  ASSERT_TRUE(SetUpProfileAndHandler());
  policy::PolicyMap policies;
  EXPECT_CALL(policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(policies));
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      enterprise_reporting::kCloudReportingEnabled, true);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, 1);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin};
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  expected_messages.insert(kManagementExtensionReportVisitedUrl);
#endif

  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("fake-token"));
  ASSERT_PRED_FORMAT2(MessagesToBeEQ,
                      handler_.GetReportingInfo(/*can_collect_signals=*/false),
                      expected_messages);
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

TEST_F(ManagementUIHandlerTests, LegacyTechReport) {
  ResetTestConfig();
  policy::PolicyMap policies;
  EXPECT_CALL(policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(policies));
  ASSERT_TRUE(SetUpProfileAndHandler());

  base::ListValue allowlist;
  allowlist.Append(base::Value("www.example.com"));
  profile_->GetTestingPrefService()->SetManagedPref(
      enterprise_reporting::kCloudLegacyTechReportAllowlist,
      std::make_unique<base::Value>(std::move(allowlist)));

  std::set<std::string> expected_messages = {kManagementLegacyTechReport};
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  expected_messages.insert(kManagementDeviceSignalsDisclosure);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  ASSERT_PRED_FORMAT2(MessagesToBeEQ, handler_.GetReportingInfo(),
                      expected_messages);
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
TEST_F(ManagementUIHandlerTests, ExtensionReportingInfoPoliciesMerge) {
  ResetTestConfig();
  ASSERT_TRUE(SetUpProfileAndHandler());
  policy::PolicyMap on_prem_reporting_extension_beta_policies;
  policy::PolicyMap on_prem_reporting_extension_stable_policies;

  EnablePolicy(kPolicyKeyReportUserIdData,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kManagementExtensionReportVersion,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kPolicyKeyReportUserIdData,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kPolicyKeyReportPolicyData,
               on_prem_reporting_extension_stable_policies);

  EnablePolicy(kPolicyKeyReportMachineIdData,
               on_prem_reporting_extension_stable_policies);
  EnablePolicy(kPolicyKeyReportSystemTelemetryData,
               on_prem_reporting_extension_stable_policies);
  EnablePolicy(kPolicyKeyReportUserBrowsingData,
               on_prem_reporting_extension_stable_policies);

  const policy::PolicyNamespace
      on_prem_reporting_extension_stable_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionStableId);
  const policy::PolicyNamespace
      on_prem_reporting_extension_beta_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionBetaId);

  EXPECT_CALL(policy_service_,
              GetPolicies(on_prem_reporting_extension_stable_policy_namespace))
      .WillOnce(ReturnRef(on_prem_reporting_extension_stable_policies));

  EXPECT_CALL(policy_service_,
              GetPolicies(on_prem_reporting_extension_beta_policy_namespace))
      .WillOnce(ReturnRef(on_prem_reporting_extension_beta_policies));
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      enterprise_reporting::kCloudReportingEnabled, true);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, 1);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineNameAddress,
      kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportUserBrowsingData,
      kManagementExtensionReportPerfCrash,
      kManagementLegacyTechReport};
  expected_messages.insert(kManagementExtensionReportVisitedUrl);

  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("fake-token"));
  ASSERT_PRED_FORMAT2(MessagesToBeEQ,
                      handler_.GetReportingInfo(/*can_collect_signals=*/false),
                      expected_messages);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ManagementUIHandlerTests, ManagedWebsitiesInfoNoPolicySet) {
  ASSERT_TRUE(SetUpNoDomainProfile());
  auto info = handler_.GetManagedWebsitesInfo(profile_);
  EXPECT_EQ(info.size(), 0u);
}

TEST_F(ManagementUIHandlerTests, ManagedWebsitiesInfoWebsites) {
  ASSERT_TRUE(SetUpNoDomainProfile());
  base::ListValue managed_websites;
  base::DictValue entry;
  entry.Set("origin", "https://example.com");
  managed_websites.Append(std::move(entry));
  profile_->GetPrefs()->Set(prefs::kManagedConfigurationPerOrigin,
                            base::Value(std::move(managed_websites)));
  auto info = handler_.GetManagedWebsitesInfo(profile_);
  EXPECT_EQ(info.size(), 1u);
  EXPECT_EQ(info.begin()->GetString(), "https://example.com");
}
#endif

TEST_F(ManagementUIHandlerTests, ThreatReportingInfo) {
  policy::PolicyMap chrome_policies;
  const policy::PolicyNamespace chrome_policies_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());

  ASSERT_TRUE(SetUpNoDomainProfile());

  EXPECT_CALL(policy_service_, GetPolicies(chrome_policies_namespace))
      .WillRepeatedly(ReturnRef(chrome_policies));

  // When no policies are set, nothing to report.
  auto info = handler_.GetThreatProtectionInfo(profile_);
  EXPECT_TRUE(info.FindList("info")->empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*info.FindString("description")));

  // When policies are set to uninteresting values, nothing to report.
#if !BUILDFLAG(IS_ANDROID)
  SetConnectorPolicyValue(policy::key::kOnFileAttachedEnterpriseConnector, "[]",
                          chrome_policies);
#endif
  SetConnectorPolicyValue(policy::key::kOnFileDownloadedEnterpriseConnector,
                          "[]", chrome_policies);
#if !BUILDFLAG(IS_ANDROID)
  SetConnectorPolicyValue(policy::key::kOnBulkDataEntryEnterpriseConnector,
                          "[]", chrome_policies);
  SetConnectorPolicyValue(policy::key::kOnDataCopiedEnterpriseConnector,
                          "[]", chrome_policies);
  SetConnectorPolicyValue(policy::key::kOnPrintEnterpriseConnector, "[]",
                          chrome_policies);
#endif
  SetConnectorPolicyValue(policy::key::kOnSecurityEventEnterpriseConnector,
                          "[]", chrome_policies);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, 0);

  info = handler_.GetThreatProtectionInfo(profile_);
  EXPECT_TRUE(info.FindList("info")->empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*info.FindString("description")));

  // When policies are set to values that enable the feature without a usable DM
  // token, nothing to report.
  policy::SetDMTokenForTesting(policy::DMToken::CreateInvalidToken());
#if !BUILDFLAG(IS_ANDROID)
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::FILE_ATTACHED,
      "[{\"service_provider\":\"google\"}]");
#endif
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED,
      "[{\"service_provider\":\"google\"}]");
#if !BUILDFLAG(IS_ANDROID)
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
      "[{\"service_provider\":\"google\"}]");
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::PRINT,
      "[{\"service_provider\":\"google\"}]");
#endif
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/profile_->GetPrefs(),
      /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{"extensionTelemetryEvent", {"*"}}});
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, 1);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  info = handler_.GetThreatProtectionInfo(profile_);
  EXPECT_TRUE(info.FindList("info")->empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*info.FindString("description")));

  // When policies are set to values that enable the feature with a usable DM
  // token, report them.
  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("fake-token"));

  info = handler_.GetThreatProtectionInfo(profile_);
#if BUILDFLAG(IS_ANDROID)
  const size_t expected_size = 4u;
#else
  const size_t expected_size = 7u;
#endif
  EXPECT_EQ(expected_size, info.FindList("info")->size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*info.FindString("description")));

  base::ListValue expected_info;
#if !BUILDFLAG(IS_ANDROID)
  {
    base::DictValue value;
    value.Set("title", kManagementOnFileAttachedEvent);
    value.Set("permission", kManagementOnFileAttachedVisibleData);
    expected_info.Append(std::move(value));
  }
#endif
  {
    base::DictValue value;
    value.Set("title", kManagementOnFileDownloadedEvent);
    value.Set("permission", kManagementOnFileDownloadedVisibleData);
    expected_info.Append(std::move(value));
  }
#if !BUILDFLAG(IS_ANDROID)
  {
    base::DictValue value;
    value.Set("title", kManagementOnBulkDataEntryEvent);
    value.Set("permission", kManagementOnBulkDataEntryVisibleData);
    expected_info.Append(std::move(value));
  }
  {
    base::DictValue value;
    value.Set("title", kManagementOnPrintEvent);
    value.Set("permission", kManagementOnPrintVisibleData);
    expected_info.Append(std::move(value));
  }
#endif
  {
    base::DictValue value;
    value.Set("title", kManagementEnterpriseReportingEvent);
    value.Set("permission", kManagementEnterpriseReportingVisibleData);
    expected_info.Append(std::move(value));
  }
  {
    base::DictValue value;
    value.Set("title", kManagementOnPageVisitedEvent);
    value.Set("permission", kManagementOnPageVisitedVisibleData);
    expected_info.Append(std::move(value));
  }
  {
    base::DictValue value;
    value.Set("title", kManagementOnExtensionTelemetryEvent);
    value.Set("permission", kManagementOnExtensionTelemetryVisibleData);
    expected_info.Append(std::move(value));
  }

  EXPECT_EQ(expected_info, *info.FindList("info"));
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(ManagementUIHandlerTests, SaasReportingBrowserPolicyEnabled) {
  ResetTestConfig(false);
  GetTestConfig().saas_reporting_browser_enabled = true;
  ASSERT_TRUE(SetUpProfileAndHandler());

  base::ListValue reports = handler_.GetReportingInfo(
      /*can_collect_signals=*/false, /*is_browser=*/true);
  EXPECT_TRUE(MessagesToBeEQ("browser_reports", "expected", reports,
                             {kManagementExtensionReportVisitedUrl}));
}

TEST_F(ManagementUIHandlerTests, SaasReportingProfilePolicyEnabled) {
  ResetTestConfig(false);
  GetTestConfig().saas_reporting_profile_enabled = true;
  ASSERT_TRUE(SetUpProfileAndHandler());

  base::ListValue reports = handler_.GetReportingInfo(
      /*can_collect_signals=*/false, /*is_browser=*/true);
  EXPECT_TRUE(MessagesToBeEQ("browser_reports", "expected", reports,
                             {kManagementExtensionReportVisitedUrl}));
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
