// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/page_info_ui.h"

#include "build/buildflag.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/page_info/page_info_ui_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class MockPageInfoUiDelegate : public PageInfoUiDelegate {
 public:
  MOCK_METHOD(bool, IsBlockAutoPlayEnabled, (), (override));
  MOCK_METHOD(bool, IsMultipleTabsOpen, (), (override));
  MOCK_METHOD(void, OpenSiteSettingsFileSystem, (), (override));
  MOCK_METHOD(content::PermissionResult,
              GetPermissionResult,
              (blink::PermissionType permission),
              (override));
  MOCK_METHOD(std::optional<content::PermissionResult>,
              GetEmbargoResult,
              (ContentSettingsType type),
              (override));
  MOCK_METHOD(void,
              GetMerchantTrustInfo,
              (page_info::MerchantDataCallback callback),
              (override));
};

}  // namespace

TEST(PageInfoUITest, PermissionStateToUIString) {
  content_settings::ContentSettingsRegistry::GetInstance();
  MockPageInfoUiDelegate delegate;
  PageInfo::PermissionInfo permission_info;
  permission_info.setting = CONTENT_SETTING_ASK;

  permission_info.type = ContentSettingsType::KEYBOARD_LOCK;
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_STATE_TEXT_KEYBOARD_LOCK_ASK),
      PageInfoUI::PermissionStateToUIString(&delegate, permission_info));

  permission_info.type = ContentSettingsType::POINTER_LOCK;
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_STATE_TEXT_POINTER_LOCK_ASK),
      PageInfoUI::PermissionStateToUIString(&delegate, permission_info));
}

TEST(PageInfoUITest, GetSecurityDescriptionSafetyTipLookalike) {
  // A valid certificate on an encrypted connection is otherwise described
  // as secure; the lookalike Safety Tip takes precedence.
  PageInfoUI::IdentityInfo identity_info;
  identity_info.identity_status = PageInfo::SITE_IDENTITY_STATUS_CERT;
  identity_info.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
  identity_info.safety_tip_info = {security_state::SafetyTipStatus::kLookalike,
                                   GURL("https://google.com")};

  PageInfoUI page_info_ui;
  std::unique_ptr<PageInfoUI::SecurityDescription> description =
      page_info_ui.GetSecurityDescription(identity_info);

  ASSERT_NE(description, nullptr);
  EXPECT_EQ(description->summary_style, PageInfoUI::SecuritySummaryColor::RED);
  EXPECT_EQ(description->summary,
            l10n_util::GetStringFUTF16(IDS_PAGE_INFO_SAFETY_TIP_LOOKALIKE_TITLE,
                                       u"google.com"));
  EXPECT_EQ(description->details,
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_DESCRIPTION));
  EXPECT_EQ(description->type, PageInfoUI::SecurityDescriptionType::SAFETY_TIP);
}
