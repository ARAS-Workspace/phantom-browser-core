// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui.h"

#include "base/containers/fixed_flat_set.h"
#include "chrome/test/base/testing_profile.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class ManagementUITest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
};

TEST_F(ManagementUITest, VerifyConstructor) {
  TestingProfile profile;
  auto web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(&profile));
  content::TestWebUI web_ui;
  web_ui.set_web_contents(web_contents.get());
  auto controller = std::make_unique<ManagementUI>(&web_ui);
  EXPECT_NE(controller, nullptr);
}

TEST_F(ManagementUITest, VerifyLocalizedStrings) {
  std::vector<webui::LocalizedString> localized_strings;
  ManagementUI::GetLocalizedStrings(localized_strings, false);

  bool found_title = false;
  for (const auto& str : localized_strings) {
    if (std::string_view(str.name) == "title") {
      found_title = true;
    }
  }
  EXPECT_TRUE(found_title);
}
