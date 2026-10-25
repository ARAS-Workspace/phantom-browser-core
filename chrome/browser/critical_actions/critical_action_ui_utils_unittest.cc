// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/critical_action_ui_utils.h"

#include "base/strings/strcat.h"
#include "chrome/common/webui_url_constants.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace critical_actions {
namespace {

TEST(CriticalActionUiUtilsTest, LinkoutsForActionTypes) {
  CriticalActionEntry form_fill;
  form_fill.action_type = ActionType::kFormFill;
  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(form_fill),
      base::StrCat({chrome::kChromeUISettingsURL, chrome::kAddressesSubPage}));

  CriticalActionEntry download;
  download.action_type = ActionType::kDownload;
  EXPECT_EQ(GetCriticalActionLinkoutUrl(download),
            chrome::kChromeUIDownloadsURL);

  CriticalActionEntry setting;
  setting.action_type = ActionType::kSettingChange;
  setting.url = GURL("https://docs.google.com/document/d/123");
  EXPECT_EQ(GetCriticalActionLinkoutUrl(setting),
            net::AppendQueryParameter(
                GURL(base::StrCat({chrome::kChromeUISettingsURL,
                                   chrome::kSiteDetailsSubpage})),
                "site", "https://docs.google.com")
                .spec());

  CriticalActionEntry gpm_action;
  gpm_action.action_type = ActionType::kGooglePasswordManager;
  gpm_action.url = GURL("https://www.example.com/login");
  EXPECT_EQ(GetCriticalActionLinkoutUrl(gpm_action),
            chrome::kChromeUISettingsURL);

  CriticalActionEntry federated_action;
  federated_action.action_type = ActionType::kFederatedLogin;
  federated_action.url = GURL("https://www.example.com/auth");
  EXPECT_EQ(GetCriticalActionLinkoutUrl(federated_action),
            chrome::kChromeUISettingsURL);

  CriticalActionEntry otp_action;
  otp_action.action_type = ActionType::kCredentialsOtp;
  otp_action.url = GURL("https://www.example.com/otp");
  EXPECT_EQ(GetCriticalActionLinkoutUrl(otp_action),
            chrome::kChromeUISettingsURL);

  CriticalActionEntry unknown_action;
  unknown_action.action_type = ActionType::kUnknown;
  EXPECT_EQ(GetCriticalActionLinkoutUrl(unknown_action),
            chrome::kChromeUISettingsURL);
}

}  // namespace
}  // namespace critical_actions
