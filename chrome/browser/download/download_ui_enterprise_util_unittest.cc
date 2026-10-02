// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_enterprise_util.h"

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {
namespace {

class DownloadUiEnterpriseUtilTest : public ::testing::Test {
 public:
  DownloadUiEnterpriseUtilTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  DownloadUiEnterpriseUtilTest(const DownloadUiEnterpriseUtilTest&) = delete;
  DownloadUiEnterpriseUtilTest& operator=(const DownloadUiEnterpriseUtilTest&) =
      delete;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));
  }

  void TearDown() override { profile_ = nullptr; }

 protected:
  raw_ptr<TestingProfile> profile_ = nullptr;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
};

TEST_F(DownloadUiEnterpriseUtilTest, DoesDownloadConnectorBlock) {
  EXPECT_FALSE(DoesDownloadConnectorBlock(profile_, GURL()));
}

}  // namespace
}  // namespace download
