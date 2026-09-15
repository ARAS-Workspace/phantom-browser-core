// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_security_view_info.h"

#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/vector_icons.h"

using DownloadUIModelPtr = DownloadUIModel::DownloadUIModelPtr;
using download::DownloadItem;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

class DownloadBubbleSecurityViewInfoTest
    : public ::testing::Test,
      public DownloadBubbleSecurityViewInfoObserver {
 public:
  DownloadBubbleSecurityViewInfoTest() = default;

  void SetUp() override {
    item_ = std::make_unique<NiceMock<download::MockDownloadItem>>();
    ON_CALL(*item_, GetGuid())
        .WillByDefault(ReturnRefOfCopy(std::string("id")));
    ON_CALL(*item_, GetURL())
        .WillByDefault(ReturnRefOfCopy(GURL("http://example.com/foo.bar")));
    content::DownloadItemUtils::AttachInfoForTesting(item_.get(), &profile_,
                                                     nullptr);
    info_ = std::make_unique<DownloadBubbleSecurityViewInfo>();
  }

  NiceMock<download::MockDownloadItem>& item() { return *item_; }
  DownloadBubbleSecurityViewInfo& info() { return *info_; }
  Profile* profile() { return &profile_; }
  TestingProfile& testing_profile() { return profile_; }

  void RefreshInfo() { info_->PopulateForDownload(item_.get()); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<NiceMock<download::MockDownloadItem>> item_;
  DownloadUIModelPtr model_;
  std::unique_ptr<DownloadBubbleSecurityViewInfo> info_;
};

// TODO: Remove the following test fixture once the ChromeRefresh flags are
//       removed or they're on by default.
class DownloadBubbleSecurityViewInfoTestGM3
    : public DownloadBubbleSecurityViewInfoTest {
 public:
  DownloadBubbleSecurityViewInfoTestGM3() = default;
  ~DownloadBubbleSecurityViewInfoTestGM3() override = default;

  void SetUp() override {
    DownloadBubbleSecurityViewInfoTest::SetUp();
    if (IsSkipped()) {
      return;
    }
  }
};

TEST_F(DownloadBubbleSecurityViewInfoTest, DangerousWarningInfo) {
  const struct DangerTypeTestCase {
    download::DownloadDangerType danger_type;
    std::optional<DownloadCommands::Command> primary_button_command;
    std::vector<DownloadCommands::Command> subpage_button_commands;
  } kDangerTypeTestCases[] = {
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
       std::nullopt,
       {DownloadCommands::Command::DISCARD, DownloadCommands::Command::KEEP}},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
       std::nullopt,
       {DownloadCommands::Command::DISCARD}},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST,
       std::nullopt,
       {DownloadCommands::Command::DISCARD}},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
       std::nullopt,
       {DownloadCommands::Command::DISCARD}},
      {download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
       std::nullopt,
       {DownloadCommands::Command::DISCARD}},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
       std::nullopt,
       {DownloadCommands::Command::DISCARD}},
      {download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING,
       DownloadCommands::Command::DISCARD,
       {DownloadCommands::Command::DISCARD, DownloadCommands::Command::KEEP}},
      {download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
       std::nullopt,
       {DownloadCommands::Command::DISCARD, DownloadCommands::Command::KEEP}},
      {download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING,
       std::nullopt,
       {DownloadCommands::Command::DEEP_SCAN,
        DownloadCommands::Command::BYPASS_DEEP_SCANNING}},
      {download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING,
       std::nullopt,
       {DownloadCommands::Command::DISCARD,
        DownloadCommands::Command::CANCEL_DEEP_SCAN}},
  };
  for (const auto& test_case : kDangerTypeTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Failed for danger type "
                 << download::GetDownloadDangerTypeString(test_case.danger_type)
                 << std::endl);
    ON_CALL(item(), GetDangerType())
        .WillByDefault(Return(test_case.danger_type));
    RefreshInfo();
    std::vector<DownloadCommands::Command> subpage_commands;
    if (info().has_primary_button()) {
      subpage_commands.push_back(info().primary_button().command);
    }
    if (info().has_secondary_button()) {
      subpage_commands.push_back(info().secondary_button().command);
    }
    EXPECT_EQ(subpage_commands, test_case.subpage_button_commands);
  }
}

TEST_F(DownloadBubbleSecurityViewInfoTestGM3, InterruptedInfo) {
  std::vector<download::DownloadInterruptReason> no_retry_interrupt_reasons = {
      download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT};
  std::vector<download::DownloadInterruptReason> retry_interrupt_reasons = {
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED,
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT,
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED,
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR,
      download::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN,
      download::DOWNLOAD_INTERRUPT_REASON_CRASH,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT};

  const struct TestCase {
    std::vector<download::DownloadInterruptReason> interrupt_reasons;
    bool can_resume;

    std::string expected_warning_summary;
    raw_ptr<const gfx::VectorIcon> expected_icon_model_override;
    std::optional<DownloadCommands::Command> expected_primary_button_command;
  } kTestCases[] = {
      {{download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED},
       false,
       "Your organization blocked this file because it didn't meet a security "
       "policy",
       &(features::IsRoundedIconsEnabled() ? views::kInfoIcon
                                           : views::kInfoChromeRefreshOldIcon),
       std::optional<DownloadCommands::Command>()},
      {{download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG},
       false,
       "Try using a shorter file name or saving to a different folder",
       &(features::IsRoundedIconsEnabled()
             ? vector_icons::kFileDownloadOffIcon
             : vector_icons::kFileDownloadOffChromeRefreshOldIcon),
       std::optional<DownloadCommands::Command>()},
      {{download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE},
       false,
       "Free up space on your device. Then, try to download again",
       &(features::IsRoundedIconsEnabled()
             ? vector_icons::kFileDownloadOffIcon
             : vector_icons::kFileDownloadOffChromeRefreshOldIcon),
       std::optional<DownloadCommands::Command>()},
      {{download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED},
       false,
       "Try to sign in to the site. Then, download again",
       &(features::IsRoundedIconsEnabled()
             ? vector_icons::kFileDownloadOffIcon
             : vector_icons::kFileDownloadOffChromeRefreshOldIcon),
       std::optional<DownloadCommands::Command>()},
      {no_retry_interrupt_reasons, false, "",
       &(features::IsRoundedIconsEnabled()
             ? vector_icons::kFileDownloadOffIcon
             : vector_icons::kFileDownloadOffChromeRefreshOldIcon),
       std::optional<DownloadCommands::Command>()},
      {retry_interrupt_reasons, false, "",
       &(features::IsRoundedIconsEnabled()
             ? vector_icons::kFileDownloadOffIcon
             : vector_icons::kFileDownloadOffChromeRefreshOldIcon),
       DownloadCommands::Command::RETRY},
      {retry_interrupt_reasons, true, "",
       &(features::IsRoundedIconsEnabled()
             ? vector_icons::kFileDownloadOffIcon
             : vector_icons::kFileDownloadOffChromeRefreshOldIcon),
       DownloadCommands::Command::RESUME},
  };

  for (const auto& test_case : kTestCases) {
    for (const auto& interrupt_reason : test_case.interrupt_reasons) {
      EXPECT_CALL(item(), GetLastReason())
          .WillRepeatedly(Return(interrupt_reason));
      EXPECT_CALL(item(), GetState())
          .WillRepeatedly(Return(
              (interrupt_reason == download::DOWNLOAD_INTERRUPT_REASON_NONE)
                  ? DownloadItem::IN_PROGRESS
                  : DownloadItem::INTERRUPTED));
      EXPECT_CALL(item(), CanResume())
          .WillRepeatedly(Return(test_case.can_resume));
      RefreshInfo();

      EXPECT_EQ(test_case.expected_warning_summary,
                base::UTF16ToUTF8(info().warning_summary()));
      EXPECT_EQ(test_case.expected_icon_model_override,
                info().icon_model_override());
      EXPECT_EQ(kColorDownloadItemIconDangerous, info().secondary_color());
      EXPECT_FALSE(info().has_progress_bar());
    }
  }
}
