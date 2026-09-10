// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_util.h"

#include <memory>
#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pdf_extension_util {
namespace {

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

class PdfExtensionUtilTest : public testing::Test {
 public:
  PdfExtensionUtilTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(PdfExtensionUtilTest, IsPdfSaveToDriveEnabled) {
  auto* profile = profile_manager()->CreateTestingProfile("test_profile");

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chrome_pdf::features::kPdfSaveToDrive);

    content::TestWebContentsFactory factory;
    content::WebContents* web_contents = factory.CreateWebContents(profile);
    base::DictValue additional_data = GetAdditionalData(web_contents);
    EXPECT_THAT(additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(true));
    TestingProfile* otr_profile =
        TestingProfile::Builder().BuildIncognito(profile);
    content::WebContents* otr_web_contents =
        factory.CreateWebContents(otr_profile);
    base::DictValue otr_additional_data = GetAdditionalData(otr_web_contents);
    EXPECT_THAT(otr_additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(false));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(chrome_pdf::features::kPdfSaveToDrive);

    content::TestWebContentsFactory factory;
    content::WebContents* web_contents = factory.CreateWebContents(profile);
    base::DictValue additional_data = GetAdditionalData(web_contents);
    EXPECT_THAT(additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(false));
  }
}

#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

}  // namespace
}  // namespace pdf_extension_util
