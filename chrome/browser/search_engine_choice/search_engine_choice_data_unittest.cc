// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_data.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void SetUserSelectedDefaultSearchProvider(
    TemplateURLService* template_url_service,
    bool created_by_policy) {
  constexpr char kCustomSearchEngineDomain[] = "bar.com";
  constexpr char16_t kCustomSearchEngineKeyword[] = u"bar.com";

  TemplateURLData data;
  data.SetShortName(kCustomSearchEngineKeyword);
  data.SetKeyword(kCustomSearchEngineKeyword);
  data.SetURL(base::StringPrintf("https://%s/url?bar={searchTerms}",
                                 kCustomSearchEngineDomain));
  data.new_tab_url =
      base::StringPrintf("https://%s/newtab", kCustomSearchEngineDomain);
  data.alternate_urls.push_back(base::StringPrintf(
      "https://%s/alt#quux={searchTerms}", kCustomSearchEngineDomain));

  if (created_by_policy) {
    data.policy_origin = TemplateURLData::PolicyOrigin::kDefaultSearchProvider;
  }

  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
}

}  // namespace

class SearchEngineChoiceDataTest : public testing::Test {
 public:
  SearchEngineChoiceDataTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Profile 1");
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  }

  TestingProfile* profile() { return profile_; }
  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

TEST_F(SearchEngineChoiceDataTest,
       GetChoiceDataFromProfilePropagatesCurrentDefault) {
  base::HistogramTester histogram_tester;
  TemplateURLService* source_template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(
      source_template_url_service);

  // Set a user-selected default search provider to ensure the source is
  // FROM_USER.
  SetUserSelectedDefaultSearchProvider(source_template_url_service,
                                       /*created_by_policy=*/false);

  const TemplateURL* underlying_default =
      source_template_url_service->GetDefaultSearchProvider();
  ASSERT_TRUE(underlying_default);
  const std::string underlying_default_url = underlying_default->url();

  std::optional<search_engines::ChoiceData> choice_data =
      search_engines::GetChoiceDataFromProfile(*profile());
  ASSERT_TRUE(choice_data.has_value());
  EXPECT_EQ(choice_data->default_search_engine.url(), underlying_default_url);

  histogram_tester.ExpectUniqueSample(
      "Search.ChoiceDebug.PropagatedDataOutcome",
      search_engines::CurrentDefaultPropagationOutcome::
          kPropagatedCurrentDefault,
      1);
}

TEST_F(SearchEngineChoiceDataTest,
       GetChoiceDataFromProfileIgnoresExtensionProvidedDefault) {
  base::HistogramTester histogram_tester;
  TemplateURLService* source_template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(
      source_template_url_service);

  // Install an extension-provided default search engine in the source profile.
  std::unique_ptr<TemplateURLData> extension_data =
      GenerateDummyTemplateURLData("extension");
  source_template_url_service->Add(std::make_unique<TemplateURL>(
      *extension_data, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION,
      "extension_id", base::Time(), /*wants_to_be_default_engine=*/true));
  SetExtensionDefaultSearchInPrefs(profile()->GetTestingPrefService(),
                                   *extension_data);
  ASSERT_TRUE(
      source_template_url_service->IsExtensionControlledDefaultSearch());

  // The choice data captured for propagation to a new profile should be absent
  // for extensions under the new logic.
  std::optional<search_engines::ChoiceData> choice_data =
      search_engines::GetChoiceDataFromProfile(*profile());
  EXPECT_EQ(choice_data, std::nullopt);

  histogram_tester.ExpectUniqueSample(
      "Search.ChoiceDebug.PropagatedDataOutcome",
      search_engines::CurrentDefaultPropagationOutcome::kSkippedIsExtension, 1);

  // Propagating absent choice data should leave the destination profile's
  // default search engine unchanged from its own default.
  TestingProfile* destination_profile =
      profile_manager()->CreateTestingProfile("Profile 2");
  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      destination_profile,
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  TemplateURLService* destination_template_url_service =
      TemplateURLServiceFactory::GetForProfile(destination_profile);
  search_test_utils::WaitForTemplateURLServiceToLoad(
      destination_template_url_service);

  const std::string destination_default_url =
      destination_template_url_service->GetDefaultSearchProvider()->url();

  search_engines::UpdateProfileFromChoiceData(*destination_profile,
                                              choice_data);
  EXPECT_FALSE(
      destination_template_url_service->IsExtensionControlledDefaultSearch());
  EXPECT_EQ(destination_template_url_service->GetDefaultSearchProvider()->url(),
            destination_default_url);
}

TEST_F(SearchEngineChoiceDataTest,
       GetChoiceDataFromProfileSkipsIfNoUnderlyingDefault) {
  base::HistogramTester histogram_tester;
  TemplateURLService* source_template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(
      source_template_url_service);

  // When no user choice is made, a fresh profile uses the fallback search
  // engine, which skips under the new FROM_FALLBACK policy.
  std::optional<search_engines::ChoiceData> choice_data =
      search_engines::GetChoiceDataFromProfile(*profile());
  EXPECT_EQ(choice_data, std::nullopt);

  histogram_tester.ExpectUniqueSample(
      "Search.ChoiceDebug.PropagatedDataOutcome",
      search_engines::CurrentDefaultPropagationOutcome::kSkippedIsFallback, 1);
}

TEST_F(SearchEngineChoiceDataTest,
       GetChoiceDataFromProfileSkipsIfManagedByPolicy) {
  base::HistogramTester histogram_tester;
  TemplateURLService* source_template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(
      source_template_url_service);

  TemplateURLData data;
  data.SetShortName(u"policy");
  data.SetKeyword(u"policy");
  data.SetURL("https://policy/url?bar={searchTerms}");
  profile()->GetTestingPrefService()->SetManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      TemplateURLDataToDictionary(data));

  ASSERT_TRUE(source_template_url_service->is_default_search_managed());

  std::optional<search_engines::ChoiceData> choice_data =
      search_engines::GetChoiceDataFromProfile(*profile());
  EXPECT_EQ(choice_data, std::nullopt);

  histogram_tester.ExpectUniqueSample(
      "Search.ChoiceDebug.PropagatedDataOutcome",
      search_engines::CurrentDefaultPropagationOutcome::kSkippedDueToPolicies,
      1);

  // Clear for subsequent tests.
  profile()->GetTestingPrefService()->RemoveManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
}
