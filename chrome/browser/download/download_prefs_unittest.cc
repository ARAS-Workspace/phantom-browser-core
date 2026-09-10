// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_prefs.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_prompt_status.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

using safe_browsing::FileTypePolicies;

namespace {

TEST(DownloadPrefsTest, Prerequisites) {
  // Most of the tests below are based on the assumption that .swf files are not
  // allowed to open automatically, and that .txt files are allowed. If this
  // assumption changes, then we need to update the tests to match.
  ASSERT_FALSE(FileTypePolicies::GetInstance()->IsAllowedToOpenAutomatically(
      base::FilePath(FILE_PATH_LITERAL("a.swf"))));
  ASSERT_TRUE(FileTypePolicies::GetInstance()->IsAllowedToOpenAutomatically(
      base::FilePath(FILE_PATH_LITERAL("a.txt"))));
}

// Verifies prefs are registered correctly.
TEST(DownloadPrefsTest, RegisterPrefs) {
  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester;

  // Download prefs are registered when creating the profile.
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

#if BUILDFLAG(IS_ANDROID)
  // Download prompt prefs should be registered correctly.
  histogram_tester.ExpectBucketCount("MobileDownload.DownloadPromptStatus",
                                     DownloadPromptStatus::SHOW_INITIAL, 1);
  int prompt_status = profile.GetTestingPrefService()->GetInteger(
      prefs::kPromptForDownloadAndroid);
  EXPECT_EQ(prompt_status,
            static_cast<int>(DownloadPromptStatus::SHOW_INITIAL));
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST(DownloadPrefsTest, NoAutoOpenByUserForDisallowedFileTypes) {
  const base::FilePath kDangerousFilePath(FILE_PATH_LITERAL("/b/very-bad.swf"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  EXPECT_FALSE(prefs.EnableAutoOpenByUserBasedOnExtension(kDangerousFilePath));
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));
}

TEST(DownloadPrefsTest, NoAutoOpenByUserForFilesWithNoExtension) {
  const base::FilePath kFileWithNoExtension(FILE_PATH_LITERAL("abcd"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  EXPECT_FALSE(
      prefs.EnableAutoOpenByUserBasedOnExtension(kFileWithNoExtension));
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kFileWithNoExtension));
}

TEST(DownloadPrefsTest, AutoOpenForSafeFiles) {
  const base::FilePath kSafeFilePath(
      FILE_PATH_LITERAL("/good/nothing-wrong.txt"));
  const base::FilePath kAnotherSafeFilePath(
      FILE_PATH_LITERAL("/ok/not-bad.txt"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  EXPECT_TRUE(prefs.EnableAutoOpenByUserBasedOnExtension(kSafeFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kSafeFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kAnotherSafeFilePath));
}

TEST(DownloadPrefsTest, AutoOpenPrefSkipsDangerousFileTypesInPrefs) {
  const base::FilePath kDangerousFilePath(FILE_PATH_LITERAL("/b/very-bad.swf"));
  const base::FilePath kSafeFilePath(
      FILE_PATH_LITERAL("/good/nothing-wrong.txt"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  // This sets .swf files and .txt files as auto-open file types.
  profile.GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen, "swf:txt");
  DownloadPrefs prefs(&profile);

  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kSafeFilePath));
}

TEST(DownloadPrefsTest, PrefsInitializationSkipsInvalidFileTypes) {
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  profile.GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen,
                                "swf:txt::.foo:baz");
  DownloadPrefs prefs(&profile);
  prefs.DisableAutoOpenByUserBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("x.baz")));

  EXPECT_FALSE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.swf"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.txt"))));
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.foo"))));

  // .swf is skipped because it's not an allowed auto-open file type.
  // The empty entry and .foo are skipped because they are malformed.
  // "baz" is removed by the DisableAutoOpenByUserBasedOnExtension() call.
  // The only entry that should be remaining is 'txt'.
  EXPECT_STREQ(
      "txt",
      profile.GetPrefs()->GetString(prefs::kDownloadExtensionsToOpen).c_str());
}

TEST(DownloadPrefsTest, AutoOpenCheckIsCaseInsensitive) {
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  profile.GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen,
                                "txt:Foo:BAR");
  DownloadPrefs prefs(&profile);

  EXPECT_TRUE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.txt"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.TXT"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.foo"))));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(
      kURL, base::FilePath(FILE_PATH_LITERAL("x.Bar"))));
}

TEST(DownloadPrefsTest, AutoOpenSetByPolicy) {
  const base::FilePath kBasicFilePath(
      FILE_PATH_LITERAL("/good/basic-path.txt"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  ScopedListPrefUpdate update(profile.GetPrefs(),
                              prefs::kDownloadExtensionsToOpenByPolicy);
  update->Append("txt");
  DownloadPrefs prefs(&profile);

  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kBasicFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kURL, kBasicFilePath));
}

TEST(DownloadPrefsTest, IsAutoOpenByPolicy) {
  const base::FilePath kFilePathType1(
      FILE_PATH_LITERAL("/good/basic-path.txt"));
  const base::FilePath kFilePathType2(
      FILE_PATH_LITERAL("/good/basic-path.exe"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  ScopedListPrefUpdate update(profile.GetPrefs(),
                              prefs::kDownloadExtensionsToOpenByPolicy);
  update->Append("exe");
  DownloadPrefs prefs(&profile);
  EXPECT_TRUE(prefs.EnableAutoOpenByUserBasedOnExtension(kFilePathType1));

  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kFilePathType1));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kFilePathType2));
  EXPECT_FALSE(prefs.IsAutoOpenByPolicy(kURL, kFilePathType1));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kURL, kFilePathType2));
}

TEST(DownloadPrefsTest, AutoOpenSetByPolicyDangerousType) {
  const base::FilePath kDangerousFilePath(
      FILE_PATH_LITERAL("/good/dangerout-type.swf"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  ScopedListPrefUpdate update(profile.GetPrefs(),
                              prefs::kDownloadExtensionsToOpenByPolicy);
  update->Append("swf");
  DownloadPrefs prefs(&profile);

  // Verifies that the user can't set this file type to auto-open, but it can
  // still be set by policy.
  EXPECT_FALSE(prefs.EnableAutoOpenByUserBasedOnExtension(kDangerousFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kURL, kDangerousFilePath));
}

TEST(DownloadPrefsTest, AutoOpenSetByPolicyDynamicUpdates) {
  const base::FilePath kDangerousFilePath(
      FILE_PATH_LITERAL("/good/dangerout-type.swf"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  // Ensure the file won't open open at first, but that it can be as soon as
  // the preference is updated.
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));

  // Update the policy preference.
  {
    ScopedListPrefUpdate update(profile.GetPrefs(),
                                prefs::kDownloadExtensionsToOpenByPolicy);
    update->Append("swf");
  }
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));

  // Remove the policy and ensure the file stops auto-opening.
  {
    ScopedListPrefUpdate update(profile.GetPrefs(),
                                prefs::kDownloadExtensionsToOpenByPolicy);
    update->clear();
  }
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kDangerousFilePath));
}

TEST(DownloadPrefsTest, AutoOpenSetByPolicyAllowedURLs) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("/good/basic-path.txt"));
  const GURL kAllowedURL("http://basic.com");
  const GURL kDisallowedURL("http://disallowed.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  ScopedListPrefUpdate update_type(profile.GetPrefs(),
                                   prefs::kDownloadExtensionsToOpenByPolicy);
  update_type->Append("txt");
  ScopedListPrefUpdate update_url(profile.GetPrefs(),
                                  prefs::kDownloadAllowedURLsForOpenByPolicy);
  update_url->Append("basic.com");
  DownloadPrefs prefs(&profile);

  // Verifies that the file only opens for the allowed url.
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kAllowedURL, kFilePath));
  EXPECT_FALSE(prefs.IsAutoOpenByPolicy(kDisallowedURL, kFilePath));
}

TEST(DownloadPrefsTest, AutoOpenSetByPolicyAllowedURLsDynamicUpdates) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("/good/basic-path.txt"));
  const GURL kAllowedURL("http://basic.com");
  const GURL kDisallowedURL("http://disallowed.com");

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  ScopedListPrefUpdate update_type(profile.GetPrefs(),
                                   prefs::kDownloadExtensionsToOpenByPolicy);
  update_type->Append("txt");
  DownloadPrefs prefs(&profile);

  // Ensure both urls work when no restrictions are present.
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kAllowedURL, kFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kDisallowedURL, kFilePath));

  // Update the policy preference to only allow |kAllowedURL|.
  {
    ScopedListPrefUpdate update_url(profile.GetPrefs(),
                                    prefs::kDownloadAllowedURLsForOpenByPolicy);
    update_url->Append("basic.com");
  }

  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kAllowedURL, kFilePath));
  EXPECT_FALSE(prefs.IsAutoOpenByPolicy(kDisallowedURL, kFilePath));

  // Remove the policy and ensure both auto-open again.
  {
    ScopedListPrefUpdate update_url(profile.GetPrefs(),
                                    prefs::kDownloadAllowedURLsForOpenByPolicy);
    update_url->clear();
  }
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kAllowedURL, kFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kDisallowedURL, kFilePath));
}

TEST(DownloadPrefsTest, AutoOpenSetByPolicyBlobURL) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("/good/basic-path.txt"));
  const GURL kAllowedURL("http://basic.com");
  const GURL kDisallowedURL("http://disallowed.com");
  const GURL kBlobAllowedURL("blob:" + kAllowedURL.spec());
  const GURL kBlobDisallowedURL("blob:" + kDisallowedURL.spec());

  ASSERT_TRUE(kBlobAllowedURL.SchemeIsBlob());
  ASSERT_TRUE(kBlobDisallowedURL.SchemeIsBlob());

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  ScopedListPrefUpdate update_type(profile.GetPrefs(),
                                   prefs::kDownloadExtensionsToOpenByPolicy);
  update_type->Append("txt");
  DownloadPrefs prefs(&profile);

  // Ensure both urls work in either form when no URL restrictions are present.
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kAllowedURL, kFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kDisallowedURL, kFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kBlobAllowedURL, kFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kBlobDisallowedURL, kFilePath));

  // Update the policy preference to only allow |kAllowedURL|.
  {
    ScopedListPrefUpdate update_url(profile.GetPrefs(),
                                    prefs::kDownloadAllowedURLsForOpenByPolicy);
    update_url->Append("basic.com");
  }

  // Ensure |kAllowedURL| continutes to work and |kDisallowedURL| is blocked,
  // even in blob form.
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kAllowedURL, kFilePath));
  EXPECT_FALSE(prefs.IsAutoOpenByPolicy(kDisallowedURL, kFilePath));
  EXPECT_TRUE(prefs.IsAutoOpenByPolicy(kBlobAllowedURL, kFilePath));
  EXPECT_FALSE(prefs.IsAutoOpenByPolicy(kBlobDisallowedURL, kFilePath));
}

TEST(DownloadPrefsTest, Pdf) {
  const base::FilePath kPdfFile(FILE_PATH_LITERAL("abcd.pdf"));
  const GURL kURL("http://basic.com");

  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  DownloadPrefs prefs(&profile);

  // Consistency check.
  EXPECT_FALSE(prefs.IsAutoOpenByUserUsed());
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kPdfFile));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  EXPECT_FALSE(prefs.ShouldOpenPdfInSystemReader());
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  prefs.SetShouldOpenPdfInSystemReader(true);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  EXPECT_TRUE(prefs.IsAutoOpenByUserUsed());
  EXPECT_TRUE(prefs.IsAutoOpenEnabled(kURL, kPdfFile));
  EXPECT_TRUE(prefs.ShouldOpenPdfInSystemReader());
#else
  EXPECT_FALSE(prefs.IsAutoOpenByUserUsed());
  EXPECT_FALSE(prefs.IsAutoOpenEnabled(kURL, kPdfFile));
  // Note ShouldOpenPdfInSystemReader is not declared on non-Desktop.
#endif
}

TEST(DownloadPrefsTest, MissingDefaultPathCorrected) {
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  profile.GetPrefs()->SetFilePath(prefs::kDownloadDefaultDirectory,
                                  base::FilePath());
  EXPECT_FALSE(profile.GetPrefs()
                   ->GetFilePath(prefs::kDownloadDefaultDirectory)
                   .IsAbsolute());

  DownloadPrefs download_prefs(&profile);
  EXPECT_TRUE(download_prefs.DownloadPath().IsAbsolute())
      << "Default download directory is " << download_prefs.DownloadPath();
}

TEST(DownloadPrefsTest, RelativeDefaultPathCorrected) {
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;

  profile.GetPrefs()->SetFilePath(prefs::kDownloadDefaultDirectory,
                                  base::FilePath::FromUTF8Unsafe(".."));
  EXPECT_FALSE(profile.GetPrefs()
                   ->GetFilePath(prefs::kDownloadDefaultDirectory)
                   .IsAbsolute());

  DownloadPrefs download_prefs(&profile);
  EXPECT_TRUE(download_prefs.DownloadPath().IsAbsolute())
      << "Default download directory is " << download_prefs.DownloadPath();
  EXPECT_EQ(
      base::ValueToFilePath(*(profile.GetTestingPrefService()->GetUserPref(
                                prefs::kDownloadDefaultDirectory)))
          .value(),
      download_prefs.GetDefaultDownloadDirectory());
}

TEST(DownloadPrefsTest, ManagedRelativePathDoesNotChangeUserPref) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kDownloadDefaultDirectory,
      base::FilePathToValue(base::FilePath::FromUTF8Unsafe("..")));
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kSaveFileDefaultDirectory,
      base::FilePathToValue(base::FilePath::FromUTF8Unsafe("../../../")));
  EXPECT_FALSE(profile.GetPrefs()
                   ->GetFilePath(prefs::kDownloadDefaultDirectory)
                   .IsAbsolute());
  EXPECT_FALSE(profile.GetPrefs()
                   ->GetFilePath(prefs::kSaveFileDefaultDirectory)
                   .IsAbsolute());

  DownloadPrefs download_prefs(&profile);
  EXPECT_FALSE(profile.GetTestingPrefService()->GetUserPref(
      prefs::kDownloadDefaultDirectory));
  EXPECT_FALSE(profile.GetTestingPrefService()->GetUserPref(
      prefs::kSaveFileDefaultDirectory));
}

TEST(DownloadPrefsTest, RecommendedRelativePathDoesNotChangeUserPref) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  base::FilePath save_dir = DownloadPrefs::GetDefaultDownloadDirectory().Append(
      base::FilePath::FromUTF8Unsafe("tmp"));
  profile.GetTestingPrefService()->SetRecommendedPref(
      prefs::kDownloadDefaultDirectory,
      base::FilePathToValue(base::FilePath::FromUTF8Unsafe("..")));
  profile.GetTestingPrefService()->SetRecommendedPref(
      prefs::kSaveFileDefaultDirectory,
      base::FilePathToValue(base::FilePath::FromUTF8Unsafe("../../../")));
  profile.GetTestingPrefService()->SetUserPref(prefs::kSaveFileDefaultDirectory,
                                               base::FilePathToValue(save_dir));
  EXPECT_FALSE(profile.GetPrefs()
                   ->GetFilePath(prefs::kDownloadDefaultDirectory)
                   .IsAbsolute());
  EXPECT_EQ(profile.GetPrefs()->GetFilePath(prefs::kSaveFileDefaultDirectory),
            save_dir);

  DownloadPrefs download_prefs(&profile);
  EXPECT_FALSE(profile.GetTestingPrefService()->GetUserPref(
      prefs::kDownloadDefaultDirectory));
  EXPECT_EQ(base::ValueToFilePath(profile.GetTestingPrefService()->GetUserPref(
                                      prefs::kSaveFileDefaultDirectory))
                .value(),
            save_dir);

  EXPECT_EQ(download_prefs.DownloadPath(),
            DownloadPrefs::GetDefaultDownloadDirectory());
  EXPECT_EQ(download_prefs.SaveFilePath(), save_dir);
}

TEST(DownloadPrefsTest, DefaultPathChangedToInvalidValue) {
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  profile.GetPrefs()->SetFilePath(prefs::kDownloadDefaultDirectory,
                                  profile.GetPath());
  EXPECT_TRUE(profile.GetPrefs()
                  ->GetFilePath(prefs::kDownloadDefaultDirectory)
                  .IsAbsolute());

  DownloadPrefs download_prefs(&profile);
  EXPECT_TRUE(download_prefs.DownloadPath().IsAbsolute());

  download_prefs.SetDownloadPath(base::FilePath::FromUTF8Unsafe(".."));
  EXPECT_EQ(download_prefs.DownloadPath(),
            download_prefs.GetDefaultDownloadDirectory());
}

#if BUILDFLAG(IS_ANDROID)
// Verifies the returned value of PromptForDownload()
// when prefs::kPromptForDownload is managed by enterprise policy,
TEST(DownloadPrefsTest, ManagedPromptForDownload) {
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kPromptForDownload, std::make_unique<base::Value>(true));
  DownloadPrefs prefs(&profile);

  profile.GetPrefs()->SetInteger(
      prefs::kPromptForDownloadAndroid,
      static_cast<int>(DownloadPromptStatus::DONT_SHOW));
  EXPECT_TRUE(prefs.PromptForDownload());

  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kPromptForDownload, std::make_unique<base::Value>(false));
  EXPECT_FALSE(prefs.PromptForDownload());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
