// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/profile_info_handler.h"

#include <memory>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "net/base/data_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace settings {

namespace {

class TestProfileInfoHandler : public ProfileInfoHandler {
 public:
  explicit TestProfileInfoHandler(Profile* profile)
      : ProfileInfoHandler(profile) {}

  using ProfileInfoHandler::set_web_ui;
};

}  // namespace

class ProfileInfoHandlerTest : public testing::Test {
 public:
  ProfileInfoHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        profile_(nullptr) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    profile_ = profile_manager_.CreateTestingProfile("Profile 1");

    handler_ = std::make_unique<TestProfileInfoHandler>(profile_);
    handler_->set_web_ui(&web_ui_);
  }

  void VerifyProfileInfo(const base::Value* call_argument) {
    ASSERT_TRUE(call_argument->is_dict());
    const base::DictValue& dict = call_argument->GetDict();

    const std::string* name = dict.FindString("name");
    const std::string* icon_url = dict.FindString("iconUrl");
    ASSERT_TRUE(name);
    ASSERT_TRUE(icon_url);

    EXPECT_EQ("Profile 1", *name);

    std::string mime, charset, data;
    EXPECT_TRUE(net::DataURL::Parse(GURL(*icon_url), &mime, &charset, &data));

    EXPECT_EQ("image/png", mime);
    SkBitmap bitmap = gfx::PNGCodec::Decode(base::as_byte_span(data));
    EXPECT_FALSE(bitmap.isNull());
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Profile* profile() const { return profile_; }
  TestProfileInfoHandler* handler() const { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;

  raw_ptr<Profile> profile_;
  std::unique_ptr<TestProfileInfoHandler> handler_;
};

TEST_F(ProfileInfoHandlerTest, GetProfileInfo) {
  base::ListValue list_args;
  list_args.Append("get-profile-info-callback-id");
  handler()->HandleGetProfileInfo(list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());

  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ("get-profile-info-callback-id", data.arg1()->GetString());

  ASSERT_TRUE(data.arg2()->is_bool());
  EXPECT_TRUE(data.arg2()->GetBool());

  VerifyProfileInfo(data.arg3());
}

TEST_F(ProfileInfoHandlerTest, PushProfileInfo) {
  handler()->AllowJavascript();

  handler()->OnProfileAvatarChanged(base::FilePath());

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ(ProfileInfoHandler::kProfileInfoChangedEventName,
            data.arg1()->GetString());

  VerifyProfileInfo(data.arg2());
}

}  // namespace settings
