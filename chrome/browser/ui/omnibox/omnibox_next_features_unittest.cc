// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_next_features.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox {

class OmniboxNextFeaturesTest : public testing::Test {
 public:
  OmniboxNextFeaturesTest() = default;

  void SetUp() override {
    testing::Test::SetUp();
    template_url_service_test_util_ =
        std::make_unique<TemplateURLServiceFactoryTestUtil>(&profile_);
    template_url_service_test_util_->VerifyLoad();

    TemplateURLData template_url_data;
    template_url_data.SetShortName(u"Google");
    template_url_data.SetKeyword(u"google.com");
    template_url_data.SetURL("https://www.google.com/search?q={searchTerms}");
    auto template_url = std::make_unique<TemplateURL>(template_url_data);
    auto* template_url_ptr =
        template_url_service_test_util_->model()->Add(std::move(template_url));
    template_url_service_test_util_->model()
        ->SetUserSelectedDefaultSearchProvider(template_url_ptr);
  }

 protected:
  TestingProfile* profile() { return &profile_; }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<TemplateURLServiceFactoryTestUtil>
      template_url_service_test_util_;
};

TEST_F(OmniboxNextFeaturesTest, IsOmniboxEverywhereEnabled) {
  // Test with null profile.
  EXPECT_FALSE(omnibox::IsOmniboxEverywhereEnabled(nullptr));

  // Test with Google DSE and feature enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
    EXPECT_TRUE(omnibox::IsOmniboxEverywhereEnabled(profile()));
  }

  // Test with Google DSE and feature disabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(omnibox::kOmniboxEverywhere);
    EXPECT_FALSE(omnibox::IsOmniboxEverywhereEnabled(profile()));
  }

  // Set non-Google default search provider.
  TemplateURLData non_google_data;
  non_google_data.SetShortName(u"Other");
  non_google_data.SetKeyword(u"other.com");
  non_google_data.SetURL("https://www.other.com/search?q={searchTerms}");
  auto non_google_url = std::make_unique<TemplateURL>(non_google_data);
  auto* non_google_ptr =
      template_url_service_test_util_->model()->Add(std::move(non_google_url));
  template_url_service_test_util_->model()
      ->SetUserSelectedDefaultSearchProvider(non_google_ptr);

  // Test with non-Google DSE and feature enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
    EXPECT_FALSE(omnibox::IsOmniboxEverywhereEnabled(profile()));
  }

  // Test with non-Google DSE and feature disabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(omnibox::kOmniboxEverywhere);
    EXPECT_FALSE(omnibox::IsOmniboxEverywhereEnabled(profile()));
  }
}

}  // namespace omnibox
