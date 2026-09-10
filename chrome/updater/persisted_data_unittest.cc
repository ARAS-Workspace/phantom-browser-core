// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/persisted_data.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/test/test_scope.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

class PersistedDataTest : public testing::Test {
 protected:
  base::test::TaskEnvironment environment_;
};

TEST_F(PersistedDataTest, Simple) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  EXPECT_FALSE(metadata->GetProductVersion("someappid").IsValid());
  EXPECT_TRUE(metadata->GetFingerprint("someappid").empty());
  EXPECT_TRUE(metadata->GetAppIds().empty());

  metadata->SetProductVersion("someappid", base::Version("1.0"));
  EXPECT_EQ("1.0", metadata->GetProductVersion("someappid").GetString());

  metadata->SetFingerprint("someappid", "fp1");
  EXPECT_EQ("fp1", metadata->GetFingerprint("someappid"));

  // Store some more apps in prefs, in addition to "someappid". Expect only
  // the app ids for apps with valid versions to be returned.
  metadata->SetProductVersion("appid1", base::Version("2.0"));
  metadata->SetFingerprint("appid2-nopv", "somefp");
  EXPECT_FALSE(metadata->GetProductVersion("appid2-nopv").IsValid());
  const auto app_ids = metadata->GetAppIds();
  EXPECT_EQ(2u, app_ids.size());
  EXPECT_TRUE(std::ranges::contains(app_ids, "someappid"));
  EXPECT_TRUE(std::ranges::contains(app_ids, "appid1"));
  EXPECT_FALSE(std::ranges::contains(app_ids, "appid2-nopv"));  // No valid pv.

  const base::Time time1 = base::Time::FromSecondsSinceUnixEpoch(10);
  metadata->SetLastChecked(time1);
  EXPECT_EQ(metadata->GetLastChecked(), time1);
  const base::Time time2 = base::Time::FromSecondsSinceUnixEpoch(20);
  metadata->SetLastStarted(time2);
  EXPECT_EQ(metadata->GetLastStarted(), time2);
}

TEST_F(PersistedDataTest, MixedCase) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  metadata->SetProductVersion("someappid", base::Version("1.0"));
  metadata->SetProductVersion("SOMEAPPID2", base::Version("2.0"));
  EXPECT_EQ("1.0", metadata->GetProductVersion("someAPPID").GetString());
  EXPECT_EQ("1.0", metadata->GetProductVersion("someappid").GetString());
  EXPECT_EQ("2.0", metadata->GetProductVersion("someAPPID2").GetString());
  EXPECT_EQ("2.0", metadata->GetProductVersion("someappid2").GetString());
}

TEST_F(PersistedDataTest, SharedPref) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  metadata->SetProductVersion("someappid", base::Version("1.0"));
  EXPECT_EQ("1.0", metadata->GetProductVersion("someappid").GetString());

  // Now, create a new PersistedData reading from the same path, verify
  // that it loads the value.
  metadata = base::MakeRefCounted<PersistedData>(GetUpdaterScopeForTesting(),
                                                 pref.get(), nullptr);
  EXPECT_EQ("1.0", metadata->GetProductVersion("someappid").GetString());
}

TEST_F(PersistedDataTest, RemoveAppId) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  RegistrationRequest data;
  data.app_id = "someappid";
  data.lang = "somelang";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = "1.0";
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));

  ASSERT_FALSE(metadata->HasApp("someappid"));
  metadata->RegisterApp(data);

  data.app_id = "someappid2";
  data.lang = "somelang";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = "2.0";
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));

  metadata->RegisterApp(data);
  EXPECT_EQ(size_t{2}, metadata->GetAppIds().size());

  ASSERT_TRUE(metadata->HasApp("someAPPID"));
  metadata->RemoveApp("someAPPID");
  ASSERT_FALSE(metadata->HasApp("someAPPID"));
  EXPECT_EQ(size_t{1}, metadata->GetAppIds().size());

  ASSERT_TRUE(metadata->HasApp("someappid2"));
  metadata->RemoveApp("someappid2");
  ASSERT_FALSE(metadata->HasApp("someappid2"));
  EXPECT_TRUE(metadata->GetAppIds().empty());
}

TEST_F(PersistedDataTest, RegisterApp_SetFirstActive) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  RegistrationRequest data;
  data.app_id = "someappid";
  data.lang = "somelang";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = "1.0";
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));
  metadata->RegisterApp(data);
  EXPECT_EQ(metadata->GetDateLastActive("someappid"), -1);
  EXPECT_EQ(metadata->GetDateLastRollCall("someappid"), -1);

  data.version = "2.0";
  data.dla = 1221;
  data.dlrc = 1221;
  metadata->RegisterApp(data);
  EXPECT_EQ(metadata->GetDateLastActive("someappid"), 1221);
  EXPECT_EQ(metadata->GetDateLastRollCall("someappid"), 1221);

  data.version = "3.0";
  data.dla = std::nullopt;
  data.dlrc = std::nullopt;
  metadata->RegisterApp(data);
  EXPECT_EQ(metadata->GetDateLastActive("someappid"), 1221);
  EXPECT_EQ(metadata->GetDateLastRollCall("someappid"), 1221);
}

class PersistedDataRegistrationRequestTest : public PersistedDataTest {
};

TEST_F(PersistedDataRegistrationRequestTest, RegistrationRequest) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  RegistrationRequest data;
  data.app_id = "someappid";
  data.lang = "somelang";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = "1.0";
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));
  data.cohort = "testcohort";
  data.cohort_name = "testcohortname";
  data.cohort_hint = "testcohorthint";

  metadata->RegisterApp(data);
  EXPECT_TRUE(metadata->GetProductVersion("someappid").IsValid());
  EXPECT_EQ("1.0", metadata->GetProductVersion("someappid").GetString());
  EXPECT_EQ(FILE_PATH_LITERAL("some/file/path"),
            metadata->GetExistenceCheckerPath("someappid").value());
  EXPECT_EQ("arandom-ap=likethis", metadata->GetAP("someappid"));
  EXPECT_EQ("somelang", metadata->GetLang("someappid"));
  EXPECT_EQ("somebrand", metadata->GetBrandCode("someappid"));

  EXPECT_EQ("testcohort", metadata->GetCohort("someappid"));
  EXPECT_EQ("testcohortname", metadata->GetCohortName("someappid"));
  EXPECT_EQ("testcohorthint", metadata->GetCohortHint("someappid"));

}

TEST_F(PersistedDataRegistrationRequestTest, RegistrationRequestPartial) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  RegistrationRequest data;
  data.app_id = "someappid";
  data.lang = "somelang";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = "1.0";
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));
  metadata->RegisterApp(data);
  EXPECT_TRUE(metadata->GetProductVersion("someappid").IsValid());
  EXPECT_EQ("1.0", metadata->GetProductVersion("someappid").GetString());
  EXPECT_EQ(FILE_PATH_LITERAL("some/file/path"),
            metadata->GetExistenceCheckerPath("someappid").value());
  EXPECT_EQ("arandom-ap=likethis", metadata->GetAP("someappid"));
  EXPECT_EQ("somelang", metadata->GetLang("someappid"));
  EXPECT_EQ("somebrand", metadata->GetBrandCode("someappid"));

  RegistrationRequest data2;
  data2.app_id = data.app_id;
  data2.ap = "different_ap";
  metadata->RegisterApp(data2);
  EXPECT_EQ("1.0", metadata->GetProductVersion(data.app_id).GetString());
  EXPECT_EQ(FILE_PATH_LITERAL("some/file/path"),
            metadata->GetExistenceCheckerPath(data.app_id).value());
  EXPECT_EQ("different_ap", metadata->GetAP(data.app_id));
  EXPECT_EQ("somelang", metadata->GetLang("someappid"));
  EXPECT_EQ("somebrand", metadata->GetBrandCode(data.app_id));

  RegistrationRequest data3;
  data3.app_id = "someappid3";
  data3.brand_code = "somebrand";
  data3.version = "1.0";
  metadata->RegisterApp(data3);
  EXPECT_TRUE(metadata->GetProductVersion("someappid3").IsValid());
  EXPECT_EQ("1.0", metadata->GetProductVersion("someappid3").GetString());
  EXPECT_EQ(FILE_PATH_LITERAL(""),
            metadata->GetExistenceCheckerPath("someappid3").value());
  EXPECT_EQ("", metadata->GetAP("someappid3"));
  EXPECT_EQ("", metadata->GetLang("someappid3"));
  EXPECT_EQ("somebrand", metadata->GetBrandCode("someappid3"));
}

}  // namespace updater
