// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_test_util.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_controller.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/fake_tab_matcher.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/enterprise/enterprise_search_manager.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/chrome_searchbox_stats.pb.h"

using ::testing::ElementsAre;

class AutocompleteControllerTest : public testing::Test {
 public:
  AutocompleteControllerTest() : controller_(&task_environment_) {}

  void SetUp() override {
    EnterpriseSearchManager::RegisterProfilePrefs(pref_service()->registry());
  }

  void SetAutocompleteMatches(const std::vector<AutocompleteMatch>& matches) {
    controller_.internal_result_.ClearMatches();
    controller_.internal_result_.AppendMatches(matches);
  }

  void UpdateSearchboxStats() {
    controller_.UpdateSearchboxStats(&controller_.internal_result_);
  }

  void UpdateShownInSession() {
    controller_.UpdateShownInSession(&controller_.internal_result_);
  }

  void MaybeRemoveCompanyEntityImages() {
    controller_.MaybeRemoveCompanyEntityImages(&controller_.internal_result_);
  }

  bool ImageURLAndImageDominantColorIsEmpty(size_t index) {
    return controller_.internal_result_.match_at(index)->image_url.is_empty() &&
           controller_.internal_result_.match_at(index)
               ->image_dominant_color.empty();
  }

  FakeAutocompleteProviderClient* provider_client() {
    return static_cast<FakeAutocompleteProviderClient*>(
        controller_.autocomplete_provider_client());
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return static_cast<sync_preferences::TestingPrefServiceSyncable*>(
        provider_client()->GetPrefs());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeAutocompleteController controller_;
};

TEST_F(AutocompleteControllerTest, UpdateShownInSessionOmitAsyncMatches) {
  std::vector<AutocompleteMatch> matches;

  AutocompleteInput input(u"abc", 3u, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(true);
  controller_.input_ = input;

  matches.push_back(CreateSearchMatch(u"abc"));
  SetAutocompleteMatches(matches);

  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);
    ASSERT_FALSE(match->session);
  }
}

TEST_F(AutocompleteControllerTest, UpdateShownInSessionTypedThenZeroPrefix) {
  std::vector<AutocompleteMatch> matches;

  AutocompleteInput typed_input(u"abc", 3u, metrics::OmniboxEventProto::OTHER,
                                TestSchemeClassifier());
  controller_.input_ = typed_input;

  matches.push_back(CreateSearchMatch(u"abc"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_FALSE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_FALSE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.abc.com/"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_FALSE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_FALSE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.clear();

  AutocompleteInput zero_prefix_input(
      u"", 0u, metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
  zero_prefix_input.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);
  controller_.input_ = zero_prefix_input;

  matches.push_back(CreateZeroPrefixSearchMatch(u"abc"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.push_back(CreateHistoryURLMatch(
      /*destination_url=*/"https://www.abc.com/", /*is_zero_prefix=*/true));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_url_suggestions_shown_in_session);
  }
}

TEST_F(AutocompleteControllerTest, UpdateShownInSessionZeroPrefixThenTyped) {
  std::vector<AutocompleteMatch> matches;

  AutocompleteInput zero_prefix_input(
      u"", 0u, metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
  zero_prefix_input.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);
  controller_.input_ = zero_prefix_input;

  matches.push_back(CreateZeroPrefixSearchMatch(u"abc"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_FALSE(match->session->typed_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.push_back(CreateHistoryURLMatch(
      /*destination_url=*/"https://www.abc.com/", /*is_zero_prefix=*/true));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_FALSE(match->session->typed_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.clear();

  AutocompleteInput typed_input(u"abc", 3u, metrics::OmniboxEventProto::OTHER,
                                TestSchemeClassifier());
  controller_.input_ = typed_input;

  matches.push_back(CreateSearchMatch(u"abc"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.abc.com/"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_url_suggestions_shown_in_session);
  }
}

TEST_F(AutocompleteControllerTest, RemoveCompanyEntityImage) {
  base::HistogramTester histogram_tester;
  std::vector<AutocompleteMatch> matches;
  // To ablate entity image the historical match must be the first and the
  // company entity can be in any other slot.
  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.wellsfargo.com/"));
  matches.push_back(CreateSearchMatch());
  matches.push_back(
      CreateCompanyEntityMatch(/*website_uri=*/"https://www.wellsfargo.com/"));

  SetAutocompleteMatches(matches);
  ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/2));

  MaybeRemoveCompanyEntityImages();
  ASSERT_TRUE(ImageURLAndImageDominantColorIsEmpty(/*index=*/2));
  histogram_tester.ExpectBucketCount("Omnibox.CompanyEntityImageAblated", true,
                                     1);
}

TEST_F(AutocompleteControllerTest, CompanyEntityImageNotRemoved) {
  // History match is not the first suggestion. Entity's image should not be
  // removed.
  {
    base::HistogramTester histogram_tester;
    std::vector<AutocompleteMatch> matches;
    matches.push_back(CreateCompanyEntityMatch(
        /*website_uri=*/"https://www.wellsfargo.com/"));
    matches.push_back(CreateHistoryURLMatch(
        /*destination_url=*/"https://www.wellsfargo.com/"));
    matches.push_back(CreateSearchMatch());

    SetAutocompleteMatches(matches);
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/0));

    MaybeRemoveCompanyEntityImages();
    // The entity's image_url should remain as is.
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/0));
    histogram_tester.ExpectBucketCount("Omnibox.CompanyEntityImageAblated",
                                       false, 1);
  }

  // History match is the first suggestion, but there isn't a matching company
  // entity.
  {
    base::HistogramTester histogram_tester;
    std::vector<AutocompleteMatch> matches;
    matches.push_back(CreateHistoryURLMatch(
        /*destination_url=*/"https://www.wellsfargo.com/"));
    matches.push_back(
        CreateCompanyEntityMatch(/*website_uri=*/"https://www.weather.com/"));

    SetAutocompleteMatches(matches);
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/1));

    MaybeRemoveCompanyEntityImages();
    // The entity's image_url should remain as is.
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/1));
    histogram_tester.ExpectBucketCount("Omnibox.CompanyEntityImageAblated",
                                       false, 1);
  }
  // There is a company entity, but no history match.
  {
    base::HistogramTester histogram_tester;
    std::vector<AutocompleteMatch> matches;
    matches.push_back(CreateSearchMatch());
    matches.push_back(
        CreateCompanyEntityMatch(/*website_uri=*/"https://www.weather.com/"));

    SetAutocompleteMatches(matches);
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/1));

    MaybeRemoveCompanyEntityImages();
    // The entity's image_url should remain as is.
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/1));
    histogram_tester.ExpectBucketCount("Omnibox.CompanyEntityImageAblated",
                                       false, 1);
  }
}

// Desktop has some special handling for bare '@' inputs.
TEST_F(AutocompleteControllerTest, FilterMatchesForInstantKeywordWithBareAt) {
  SetAutocompleteMatches({
      CreateSearchMatch(u"@"),
      CreateCompanyEntityMatch("https://example.com"),
      CreateHistoryURLMatch("https://example.com"),
      CreateStarterPackMatch(u"@bookmarks"),
      CreateStarterPackMatch(u"@history"),
      CreateStarterPackMatch(u"@tabs"),
      CreateFeaturedEnterpriseSearch(u"@work"),
  });

  AutocompleteInput input(u"@", 1u, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  controller_.MaybeCleanSuggestionsForKeywordMode(
      input, &controller_.internal_result_);

  EXPECT_EQ(controller_.internal_result_.size(), 5u);
  EXPECT_TRUE(std::all_of(
      controller_.internal_result_.begin(), controller_.internal_result_.end(),
      [](const auto& match) {
        return match.type == AutocompleteMatchType::STARTER_PACK ||
               match.type ==
                   AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH ||
               match.contents == u"@";
      }));
}

TEST_F(AutocompleteControllerTest, UpdateResult_SyncAnd2Async) {
  auto sync_match = CreateSearchMatch("sync", true, 1300);
  auto async_match1 = CreateSearchMatch("async_1", true, 1200);
  auto async_match2 = CreateSearchMatch("async_2", true, 1250);
  {
    SCOPED_TRACE("Sync pass.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(true, false, {sync_match}),
                testing::ElementsAreArray({
                    "sync",
                }));
  }
  {
    SCOPED_TRACE("1st async pass.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    false, false, {sync_match, async_match1}),
                testing::ElementsAreArray({
                    "sync",
                    "async_1",
                }));
  }
  {
    SCOPED_TRACE(
        "Last async pass. Verify the correct matches are shown ranked by "
        "relevance.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    false, true, {async_match1, async_match2}),
                testing::ElementsAreArray({
                    "async_2",
                    "async_1",
                }));
  }
}

TEST_F(AutocompleteControllerTest, UpdateResult_TransferringOldMatches) {
  auto pass1_match1 = CreateSearchMatch("pass1_match1", true, 1300);
  auto pass1_match2 = CreateSearchMatch("pass1_match2", true, 1200);
  auto pass1_match3 = CreateSearchMatch("pass1_match3", true, 1100);
  auto pass2_match1 = CreateSearchMatch("pass2_match1", true, 1000);
  auto pass3_match2 = CreateSearchMatch("pass3_match2", true, 900);
  auto pass3_match3 = CreateSearchMatch("pass3_match3", true, 800);
  auto pass3_match4 = CreateSearchMatch("pass3_match4", true, 700);
  auto pass4_match1 = CreateSearchMatch("pass4_match1", true, 600);
  auto pass5_match1 = CreateSearchMatch("pass5_match1", true, 500);
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  true, false, {pass1_match1, pass1_match2, pass1_match3}),
              testing::ElementsAreArray({
                  "pass1_match1",
                  "pass1_match2",
                  "pass1_match3",
              }));
  // # of matches decreased from 3 to 2. So 1 match should be transferred. The
  // lowest ranked match should be transferred. It should keep its score and be
  // ranked above the new non-transferred match.
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  false, false, {pass1_match1, pass2_match1}),
              testing::ElementsAreArray({
                  "pass1_match1",
                  "pass1_match3",
                  "pass2_match1",
              }));
  // # of matches remained 3. So no matches should be transferred.
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  false, false, {pass3_match2, pass3_match3, pass3_match4}),
              testing::ElementsAreArray({
                  "pass3_match2",
                  "pass3_match3",
                  "pass3_match4",
              }));
  // Transferred matches should not be allowed to be default.
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(false, false, {pass4_match1}),
      testing::ElementsAreArray({
          "pass4_match1",
          "pass3_match3",
          "pass3_match4",
      }));
  // Lowest ranked match should be transferred. But old matches still present
  // shouldn't count, and the next lowest match should be transferred.
  // Transferred match scores should be capped to the new default, therefore,
  // the transferred `pass3_match3` should be demoted to last even though it
  // originally outscored `pass3_match4`.
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  false, false, {pass4_match1, pass3_match4}),
              testing::ElementsAreArray({
                  "pass4_match1",
                  "pass3_match4",
                  "pass3_match3",
              }));
  // Sync updates should also transfer old matches. Lowest ranked, not
  // necessarily lowest scored, match should be transferred.
  EXPECT_THAT(controller_.SimulateAutocompletePass(true, false, {pass5_match1}),
              testing::ElementsAreArray({
                  "pass5_match1",
                  "pass3_match3",
                  "pass3_match4",
              }));
  // Expire updates should not transfer old matches.
  EXPECT_THAT(controller_.SimulateExpirePass(), testing::ElementsAreArray({
                                                    "pass5_match1",
                                                }));
  // Async updates after the expire update should transfer matches.
  EXPECT_THAT(controller_.SimulateAutocompletePass(false, false, {}),
              testing::ElementsAreArray({
                  "pass5_match1",
              }));
  // The last async pass shouldn't transfer matches.
  EXPECT_THAT(controller_.SimulateAutocompletePass(true, true, {}),
              testing::ElementsAreArray<std::string>({}));
}

TEST_F(AutocompleteControllerTest, UpdateResult_PreservingDefault) {
  auto match1 = CreateSearchMatch("match1", true, 100);
  auto match2 = CreateSearchMatch("match2", true, 200);
  auto match3 = CreateSearchMatch("match3", true, 300);
  auto match4 = CreateSearchMatch("match4", true, 400);
  auto match5 = CreateSearchMatch("match5", true, 500);
  auto match6 = CreateSearchMatch("match6", true, 600);
  auto match7 = CreateSearchMatch("match7", true, 700);
  auto match8 = CreateSearchMatch("match8", true, 800);
  auto match9 = CreateSearchMatch("match9", true, 900);
  {
    SCOPED_TRACE("Load a default suggestion.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(true, true, {match1}),
                testing::ElementsAreArray({
                    "match1",
                }));
  }
  {
    SCOPED_TRACE("Don't preserve default on sync pass with short inputs.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    true, false, {match1, match2},
                    FakeAutocompleteController::CreateInput(u"x")),
                testing::ElementsAreArray({
                    "match2",
                    "match1",
                }));
  }
  {
    SCOPED_TRACE("Preserve default on async pass with short inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, false, {match2, match3}),
        testing::ElementsAreArray({
            "match2",
            "match3",
        }));
    // Preserve default on last async pass with short inputs. Preserve default
  }
  {
    SCOPED_TRACE("across multiple passes.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, true, {match2, match4}),
        testing::ElementsAreArray({
            "match2",
            "match4",
        }));
    // Preserve default on sync pass with long inputs. Preserve default across
  }
  {
    SCOPED_TRACE("multiple inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(true, false, {match2, match5}),
        testing::ElementsAreArray({
            "match2",
            "match5",
        }));
  }
  {
    SCOPED_TRACE("Preserve default on async pass with long inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, false, {match2, match6}),
        testing::ElementsAreArray({
            "match2",
            "match6",
        }));
  }
  {
    SCOPED_TRACE("Don't preserve default if it's transferred.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, false, {match7, match8}),
        testing::ElementsAreArray({
            "match8",
            "match7",
        }));
  }
  {
    SCOPED_TRACE("Preserve default on last async pass with long inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, true, {match8, match9}),
        testing::ElementsAreArray({
            "match8",
            "match9",
        }));
  }
}

TEST_F(AutocompleteControllerTest, UpdateResult_Ranking) {
  // Higher scored suggestions are ranked higher.
  // Clear results between each test to avoid default preserving applying.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("500", true, 500),
                  CreateSearchMatch("800", true, 800),
              }),
              testing::ElementsAreArray({
                  "800",
                  "500",
              }));

  // Default suggestion must be allowed to be default.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("500", true, 500),
                  CreateSearchMatch("800", false, 800),
              }),
              testing::ElementsAreArray({
                  "500",
                  "800",
              }));

  // Searches should be grouped above non-shortcut-boosted URLs.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("search900", true, 900),
                  CreateHistoryUrlMlScoredMatch("history800", true, 800, 1),
                  CreateSearchMatch("search700", true, 700),
                  CreateHistoryUrlMlScoredMatch("history600", true, 600, 1),
                  CreateSearchMatch("search500", true, 500),
                  CreateHistoryUrlMlScoredMatch("history400", true, 400, 1),
              }),
              testing::ElementsAreArray({
                  "search900",
                  "search700",
                  "search500",
                  "history800",
                  "history600",
                  "history400",
              }));

  // Default can be a non-search if it's scored higher than all the searches.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("search900", true, 900),
                  CreateHistoryUrlMlScoredMatch("history800", true, 800, 1),
                  CreateSearchMatch("search700", true, 700),
                  CreateHistoryUrlMlScoredMatch("history600", true, 600, 1),
                  CreateSearchMatch("search500", true, 500),
                  CreateHistoryUrlMlScoredMatch("history400", true, 400, 1),
                  CreateHistoryUrlMlScoredMatch("history1000", true, 1000, 1),
              }),
              testing::ElementsAreArray({
                  "history1000",
                  "search900",
                  "search700",
                  "search500",
                  "history800",
                  "history600",
                  "history400",
              }));

  // Shortcut boosted suggestions should be ranked above searches, even if
  // they're scored lower.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateHistoryUrlMlScoredMatch("history800", true, 800, 1),
                  CreateHistoryUrlMlScoredMatch("history850", true, 850, 1),
                  CreateSearchMatch("search700", true, 700),
                  CreateSearchMatch("search750", true, 750),
                  CreateBoostedShortcutMatch("shortcut600", 600, 1),
                  CreateBoostedShortcutMatch("shortcut650", 650, 1),
              }),
              testing::ElementsAreArray({
                  "history850",
                  "shortcut650",
                  "shortcut600",
                  "search750",
                  "search700",
                  "history800",
              }));
}

TEST_F(AutocompleteControllerTest, UpdateResult_ZPSEnabledAndShownInSession) {
  // Populate TemplateURLService with a keyword.
  TemplateURLData turl_data;
  turl_data.SetShortName(u"Keyword");
  turl_data.SetKeyword(u"keyword");
  turl_data.SetURL("https://google.com/search?q={searchTerms}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(turl_data));

  // Create a zero-suggest input.
  auto zps_input = FakeAutocompleteController::CreateInput(u"");
  zps_input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);

  {
    SCOPED_TRACE("Zero-prefix suggestions are offered synchronously");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    /*sync=*/true, /*done=*/false,
                    {
                        CreatePersonalizedZeroPrefixMatch("zps_1", 1450),
                        CreatePersonalizedZeroPrefixMatch("zps_2", 1449),
                    },
                    zps_input),
                testing::ElementsAreArray({
                    "zps_1",
                    "zps_2",
                }));
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are updated in the internal result set.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              2u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              2u);
  }
  {
    SCOPED_TRACE("More zero-prefix suggestions are offered asynchronously");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    /*sync=*/false, /*done=*/false,
                    {
                        CreatePersonalizedZeroPrefixMatch("zps_1", 1450),
                        CreatePersonalizedZeroPrefixMatch("zps_2", 1449),
                        CreatePersonalizedZeroPrefixMatch("zps_3", 1448),
                        CreatePersonalizedZeroPrefixMatch("zps_4", 1447),
                    },
                    zps_input),
                testing::ElementsAreArray({
                    "zps_1",
                    "zps_2",
                    "zps_3",
                    "zps_4",
                }));
    // If zero-prefix suggestions are offered multiple times in the session, the
    // most recent count is logged.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              4u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              4u);
  }
  {
    SCOPED_TRACE("Stop with clear_result=false is called due to user idleness");
    controller_.Stop(AutocompleteStopReason::kInteraction);
    // Stop with clear_result=false does not clear the internal result set and
    // does not notify `OnResultChanged()`.
    EXPECT_FALSE(controller_.internal_result_.empty());
    EXPECT_FALSE(controller_.published_result_.empty());
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are unchanged in the internal result
    // set.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              4u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              4u);
  }
  {
    SCOPED_TRACE("Prefix suggestions are offered synchronously");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    /*sync=*/true, /*done=*/true,
                    {
                        CreateSearchMatch("search_1", true, 900),
                    }),
                testing::ElementsAreArray({
                    "search_1",
                }));
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are unchanged in the internal result
    // set.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              4u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              4u);
  }
  {
    SCOPED_TRACE("Stop with clear_result=true is called due to popup closing");
    controller_.Stop(AutocompleteStopReason::kClobbered);
    // Stop with clear_result=true clears the internal result set and notifies
    // `OnResultChanged()`.
    EXPECT_TRUE(controller_.internal_result_.empty());
    EXPECT_TRUE(controller_.published_result_.empty());
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are reset in the internal result set.
    EXPECT_FALSE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
  }
}

TEST_F(AutocompleteControllerTest, UpdateResult_NotifyingAndTimers) {
  {
    SCOPED_TRACE("Expect immediate notification after sync pass.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
  }
  {
    SCOPED_TRACE("Expect debounced notification after async pass.");
    controller_.GetFakeProvider().done_ = false;
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        200, AutocompleteController::UpdateType::kAsyncPass);
  }
  {
    SCOPED_TRACE("Expect debouncing to reset after each async passes.");
    controller_.GetFakeProvider().done_ = false;
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    task_environment_.FastForwardBy(base::Milliseconds(150));
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    task_environment_.FastForwardBy(base::Milliseconds(150));
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        200, AutocompleteController::UpdateType::kAsyncPass);
  }
  {
    SCOPED_TRACE("Expect delayed notification after expiration.");
    controller_.UpdateResult(AutocompleteController::UpdateType::kExpirePass);
    controller_.ExpectOnResultChanged(
        200, AutocompleteController::UpdateType::kExpirePass);
  }
  {
    SCOPED_TRACE("Expect immediate notification after the last async pass.");
    controller_.GetFakeProvider().done_ = true;
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kLastAsyncPass);
  }
  {
    SCOPED_TRACE("Expect no stop update after the last async pass.");
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Expect immediate notification after the last async pass, even if a "
        "debounced notification is pending.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);

    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());

    controller_.GetFakeProvider().done_ = true;
    task_environment_.FastForwardBy(base::Milliseconds(10));
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kLastAsyncPass);
  }
  {
    SCOPED_TRACE("Expect no stop update after the last async pass (2).");
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE("Expect no stop update after a sync only pass.");
    controller_.GetFakeProvider().done_ = true;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPassOnly);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Expect a stop update if the async passes takes too long. Expect no "
        "notification.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
    controller_.ExpectStopAfter(1500);
  }
  {
    SCOPED_TRACE(
        "Expect a stop update to flush any pending notification for completed "
        "non-final async passes.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
    for (size_t i = 0; i < 9; ++i) {
      task_environment_.FastForwardBy(base::Milliseconds(150));
      controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    }
    controller_.ExpectStopAfter(150);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE("Expect debounced expire notification.");
    controller_.GetFakeProvider().done_ = false;
    AutocompleteMatch transferred_match{
        nullptr, 1000, false, AutocompleteMatchType::URL_WHAT_YOU_TYPED};
    transferred_match.from_previous = true;
    controller_.GetFakeProvider().matches_ = {transferred_match};
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
    // Expire timer is 500ms. Debounce delay is 200ms.
    controller_.ExpectOnResultChanged(
        700, AutocompleteController::UpdateType::kExpirePass);
    controller_.ExpectStopAfter(800);
    controller_.ExpectNoNotificationOrStop();
  }
}

TEST_F(AutocompleteControllerTest, ExplicitStop) {
  // Besides the `Stop()` fired by the timer, which is tested in
  // `UpdateResult_NotifyingAndTimers`, there's also user triggered `Stop()`s
  // tests here.

  auto matches = {CreateSearchMatch("search", true, 900)};

  {
    SCOPED_TRACE(
        "Stop with `kInteraction` and no pending changes should not notify "
        "`OnResultChanged()` - there's no change to notify of.");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.Stop(AutocompleteStopReason::kInteraction);
    controller_.ExpectStopAfter(0, true);
    EXPECT_FALSE(controller_.published_result_.empty());
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Stop with `kInteraction` and pending changes should not notify "
        "`OnResultChanged()` - the last pending change should be "
        "abandoned to avoid changes as the user's e.g. down arrowing.");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.SimulateAutocompletePass(false, false, matches);
    controller_.Stop(AutocompleteStopReason::kInteraction);
    EXPECT_FALSE(controller_.published_result_.empty());
    controller_.ExpectStopAfter(0, true);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Stop with `kClobbered` and no pending notifications should notify "
        "`OnResultChanged()` - observers should know the results were "
        "cleared.");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.observer_->last_default_match_changed = true;
    controller_.Stop(AutocompleteStopReason::kClobbered);
    EXPECT_TRUE(controller_.published_result_.empty());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kStop);
    EXPECT_FALSE(controller_.observer_->last_default_match_changed);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Stop with `kClobbered` and pending notifications should notify "
        "`OnResultChanged()` - observers should know the results were cleared."
        "cleared.");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.SimulateAutocompletePass(false, false, matches);
    controller_.observer_->last_default_match_changed = true;
    controller_.Stop(AutocompleteStopReason::kClobbered);
    EXPECT_TRUE(controller_.published_result_.empty());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kStop);
    EXPECT_FALSE(controller_.observer_->last_default_match_changed);
    controller_.ExpectNoNotificationOrStop();
  }
}

TEST_F(AutocompleteControllerTest, UpdateResult_ForceAllowedToBeDefault) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kGroupingFrameworkForNonZPS);

  auto set_feature = [](bool enabled) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(
        omnibox_feature_configs::ForceAllowedToBeDefault::
            kForceAllowedToBeDefault,
        enabled);
    return omnibox_feature_configs::ScopedConfigForTesting<
        omnibox_feature_configs::ForceAllowedToBeDefault>();
  };

  {
    // When disabled, a not-defaultable history match should not be default.
    SCOPED_TRACE("Disabled");
    auto disabled_config = set_feature(false);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateHistoryUrlMlScoredMatch("history", false, 1400, .5),
                }),
                testing::ElementsAreArray({
                    "search",
                    "history",
                }));
  }
  {
    // An initially not-defaultable history match can be made defaultable.
    SCOPED_TRACE("Enabled");
    auto enabled_config = set_feature(true);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateHistoryUrlMlScoredMatch("history", false, 1400, .5),
                }),
                testing::ElementsAreArray({
                    "history",
                    "search",
                }));
  }
  {
    // Initially defaultable matches should not be made non-defaultable even if
    // they don't qualify for forcing defaultable.
    SCOPED_TRACE("Enabled defaultable");
    auto enabled_config = set_feature(true);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateHistoryUrlMlScoredMatch("history", true, 300, .5),
                }),
                testing::ElementsAreArray({
                    "history",
                    "search",
                }));
  }
  {
    // Keyword matches shouldn't be made defaultable.
    SCOPED_TRACE("Enabled keyword");
    auto enabled_config = set_feature(true);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateSearchMatch("keyword", false, 1000),
                }),
                testing::ElementsAreArray({
                    "search",
                    "keyword",
                }));
  }
  {
    // Should not force default when `prevent_inline_autocomplete_` is true.
    SCOPED_TRACE("Enabled prevent inline autocomplete");
    auto enabled_config = set_feature(true);
    controller_.internal_result_.ClearMatches();
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(
            true, true,
            {
                CreateSearchMatch("search", true, 200),
                CreateAutocompleteMatch("history",
                                        AutocompleteMatchType::HISTORY_CLUSTER,
                                        false, false, 1000, std::nullopt),
            },
            FakeAutocompleteController::CreateInput(u"test", false, true)),
        testing::ElementsAreArray({
            "search",
            "history",
        }));
  }
}

TEST_F(AutocompleteControllerTest, ShouldRunProvider_StarterPack) {
  std::set<AutocompleteProvider::Type> expected_provider_types;
  AutocompleteInput input(u"a", 1u, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  controller_.input_ = input;

  // Populate template URL service with starter pack entries.
  std::vector<std::unique_ptr<TemplateURLData>> turls =
      template_url_starter_pack_data::GetStarterPackEngines();
  for (auto& turl : turls) {
    controller_.template_url_service_->Add(
        std::make_unique<TemplateURL>(std::move(*turl)));
  }

  // Not in keyword mode, run all providers except open tab provider.
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              provider->type() != AutocompleteProvider::TYPE_OPEN_TAB)
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // Enter keyword mode.
  controller_.input_.set_in_keyword_mode(true);

  // In @tabs, run search, keyword, and open tab provider only.
  controller_.input_.UpdateText(u"@tabs", 0, {});
  expected_provider_types = {AutocompleteProvider::TYPE_KEYWORD,
                             AutocompleteProvider::TYPE_SEARCH,
                             AutocompleteProvider::TYPE_OPEN_TAB};
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // In @bookmarks, run search, keyword, and bookmarks only.
  controller_.input_.UpdateText(u"@bookmarks", 0, {});
  expected_provider_types = {AutocompleteProvider::TYPE_KEYWORD,
                             AutocompleteProvider::TYPE_SEARCH,
                             AutocompleteProvider::TYPE_BOOKMARK};
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // In @history, run search, keyword, and history providers only.
  controller_.input_.UpdateText(u"@history", 0, {});
  expected_provider_types = {AutocompleteProvider::TYPE_KEYWORD,
                             AutocompleteProvider::TYPE_SEARCH,
                             AutocompleteProvider::TYPE_HISTORY_QUICK,
                             AutocompleteProvider::TYPE_HISTORY_URL};
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
}

TEST_F(AutocompleteControllerTest,
       ShouldRunProvider_LimitKeywordModeSuggestions) {
  std::set<AutocompleteProvider::Type> excluded_provider_types;
  AutocompleteInput input(u"a", 1u, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  controller_.input_ = input;

  // Populate template URL service with an entry for drive.google.com (to test
  // document provider) and a generic keyword entry.
  TemplateURLData drive_turl_data;
  drive_turl_data.SetShortName(u"Google Drive");
  drive_turl_data.SetKeyword(u"drive.google.com");
  drive_turl_data.SetURL("https://drive.google.com/search?q={searchTerms}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(drive_turl_data));
  TemplateURLData turl_data;
  turl_data.SetShortName(u"Test Keyword");
  turl_data.SetKeyword(u"keyword");
  turl_data.SetURL("https://google.com/search?q={searchTerms}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(turl_data));

  // Not in keyword mode, run all providers except open tab provider.
  excluded_provider_types = {AutocompleteProvider::TYPE_OPEN_TAB};
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // In keyword mode, all limit provider params on by default, limit document
  // and history cluster suggestions as well.
  controller_.input_.UpdateText(u"keyword", 0, {});
  controller_.input_.set_in_keyword_mode(true);
  excluded_provider_types = {
      AutocompleteProvider::TYPE_OPEN_TAB,
      AutocompleteProvider::TYPE_HISTORY_CLUSTER_PROVIDER,
      AutocompleteProvider::TYPE_DOCUMENT,
      AutocompleteProvider::TYPE_ON_DEVICE_HEAD};
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // For drive.google.com, run document provider.
  controller_.input_.UpdateText(u"drive.google.com", 0, {});
  excluded_provider_types = {
      AutocompleteProvider::TYPE_OPEN_TAB,
      AutocompleteProvider::TYPE_HISTORY_CLUSTER_PROVIDER,
      AutocompleteProvider::TYPE_ON_DEVICE_HEAD};
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
}

TEST_F(AutocompleteControllerTest, ShouldRunProvider_LensSearchbox) {
  // Run all providers except open tab provider.
  std::set<AutocompleteProvider::Type> excluded_provider_types = {
      AutocompleteProvider::TYPE_OPEN_TAB};
  controller_.input_ = AutocompleteInput(
      u"a", 1u, metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // For Lens searchboxes, run search provider only.
  std::set<AutocompleteProvider::Type> expected_provider_types = {
      AutocompleteProvider::TYPE_SEARCH};

  controller_.input_ = AutocompleteInput(
      u"a", 1u, metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX,
      TestSchemeClassifier());
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  controller_.input_ = AutocompleteInput(
      u"a", 1u, metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX,
      TestSchemeClassifier());
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  controller_.input_ = AutocompleteInput(
      u"a", 1u, metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX,
      TestSchemeClassifier());
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
}

// The EnterpriseSearchAggregatorProvider is only run on desktop.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(AutocompleteControllerTest,
       ShouldRunProvider_EnterpriseSearchAggregator) {
  // Populate template URL service.
  auto add_template_url = [&](const std::string& name,
                              TemplateURLData::PolicyOrigin policy_origin,
                              bool featured) {
    TemplateURLData data;
    data.SetShortName(base::UTF8ToUTF16(name));
    data.SetKeyword(base::UTF8ToUTF16(name));
    data.SetURL("https://" + name + ".com/q={searchTerms}");
    data.policy_origin = policy_origin;
    data.featured_by_policy = featured;
    controller_.template_url_service_->Add(std::make_unique<TemplateURL>(data));
  };

  add_template_url("site_search_not_featured",
                   TemplateURLData::PolicyOrigin::kSiteSearch, false);
  add_template_url("site_search_featured",
                   TemplateURLData::PolicyOrigin::kSiteSearch, true);
  add_template_url("aggregator_not_featured",
                   TemplateURLData::PolicyOrigin::kSearchAggregator, false);
  add_template_url("aggregator_featured",
                   TemplateURLData::PolicyOrigin::kSearchAggregator, true);

  // Setup the providers.
  auto aggregator_provider = base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_ENTERPRISE_SEARCH_AGGREGATOR);
  controller_.providers_.push_back(aggregator_provider);
  auto document_provider = base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_DOCUMENT);
  controller_.providers_.push_back(document_provider);

  // In unscoped mode (not keyword mode), aggregator is run when
  // `require_shortcut` policy field is false, and is not run when
  // `require_shortcut` policy field is true. When it is run, the document
  // provider should not be run and vice versa.
  controller_.input_ = AutocompleteInput(
      u"query", 1u, metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
  EXPECT_TRUE(controller_.ShouldRunProvider(aggregator_provider.get()));
  EXPECT_FALSE(controller_.ShouldRunProvider(document_provider.get()));

  pref_service()->SetManagedPref(
      EnterpriseSearchManager::
          kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName,
      base::Value(true));
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));
  EXPECT_TRUE(controller_.ShouldRunProvider(document_provider.get()));

  // If the feature param `disable_drive` is false, then the document provider
  // should run regardless of whether the aggregator provider is ran.
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SearchAggregatorProvider>
      scoped_config;
  scoped_config.Get().disable_drive = false;
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::
          kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName,
      base::Value(false));
  EXPECT_TRUE(controller_.ShouldRunProvider(aggregator_provider.get()));
  EXPECT_TRUE(controller_.ShouldRunProvider(document_provider.get()));

  pref_service()->SetManagedPref(
      EnterpriseSearchManager::
          kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName,
      base::Value(true));
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));
  EXPECT_TRUE(controller_.ShouldRunProvider(document_provider.get()));

  // Enter keyword mode.
  controller_.input_.set_in_keyword_mode(true);

  // Aggregator not ran when in site search mode, regardless of
  // `enterprise_search_aggregator_settings.require_shortcut` pref value.
  controller_.input_.UpdateText(u"site_search_not_featured", 0, {});
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));
  controller_.input_.UpdateText(u"site_search_featured", 0, {});
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));

  pref_service()->SetManagedPref(
      EnterpriseSearchManager::
          kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName,
      base::Value(false));
  controller_.input_.UpdateText(u"site_search_not_featured", 0, {});
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));
  controller_.input_.UpdateText(u"site_search_featured", 0, {});
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));

  // Only search, keyword, and aggregator providers ran when in aggregator mode.
  std::set<AutocompleteProvider::Type> expected_provider_types = {
      AutocompleteProvider::TYPE_SEARCH, AutocompleteProvider::TYPE_KEYWORD,
      AutocompleteProvider::Type::TYPE_ENTERPRISE_SEARCH_AGGREGATOR};
  controller_.input_.UpdateText(u"aggregator_not_featured", 0, {});
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
  controller_.input_.UpdateText(u"aggregator_featured", 0, {});
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

TEST_F(AutocompleteControllerTest, NoActionsAttachedToLensSearchboxMatches) {
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    pedals.insert(
        std::make_pair(pedal->PedalId(), base::WrapRefCounted(pedal)));
  };
  add(new TestOmniboxPedalClearBrowsingData());
  provider_client()->set_pedal_provider(std::make_unique<OmniboxPedalProvider>(
      *provider_client(), std::move(pedals)));
  EXPECT_NE(nullptr, provider_client()->GetPedalProvider());

  // Create input with lens searchbox page classification.
  controller_.input_ = AutocompleteInput(
      u"Clear History", metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX,
      TestSchemeClassifier());

  SetAutocompleteMatches(
      {CreateSearchMatch(u"Clear History"), CreateSearchMatch(u"search 1"),
       CreateSearchMatch(u"search 2"),
       CreateHistoryURLMatch(
           /*destination_url=*/"http://this-site-matches.com")});

  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(provider_client()->GetTabMatcher()))
      .set_url_substring_match("matches");

  controller_.AttachActions();

  // For a Lens Searchbox, AttachActions should not attach a pedal to the
  // first match, and therefore it won't get split out into a separate pedal
  // match. It also shouldn't attach a switch to this tab action to the last
  // match.
  EXPECT_EQ(nullptr, controller_.internal_result_.match_at(1)->takeover_action);
  EXPECT_FALSE(
      controller_.internal_result_.match_at(3)->has_tab_match.value_or(false));

  controller_.input_ =
      AutocompleteInput(u"Clear History", metrics::OmniboxEventProto::OTHER,
                        TestSchemeClassifier());

  SetAutocompleteMatches(
      {CreateSearchMatch(u"Clear History"),
       CreateHistoryURLMatch(
           /*destination_url=*/"http://this-site-matches.com"),
       CreateSearchMatch(u"search 1"), CreateSearchMatch(u"search 2")});

  controller_.AttachActions();

  // For any other page classification, AttachActions should attach a pedal
  // and a switch to this tab action to the relevant matches.
  EXPECT_EQ(
      OmniboxActionId::PEDAL,
      controller_.internal_result_.match_at(1)->takeover_action->ActionId());
  EXPECT_TRUE(
      controller_.internal_result_.match_at(2)->has_tab_match.value_or(false));
}

TEST_F(AutocompleteControllerTest, NoActionsAttachedToNtpComposeboxMatches) {
  // Create input with lens searchbox page classification.
  controller_.input_ = AutocompleteInput(
      u"Clear History", metrics::OmniboxEventProto::NTP_COMPOSEBOX,
      TestSchemeClassifier());

  SetAutocompleteMatches(
      {CreateSearchMatch(u"search 2"),
       CreateHistoryURLMatch(
           /*destination_url=*/"http://this-site-matches.com")});

  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(provider_client()->GetTabMatcher()))
      .set_url_substring_match("matches");

  controller_.AttachActions();

  // For a Lens Searchbox, AttachActions shouldn't attach a switch to this tab
  // action to the last match.
  EXPECT_FALSE(
      controller_.internal_result_.match_at(1)->has_tab_match.value_or(false));

  controller_.input_ =
      AutocompleteInput(u"Clear History", metrics::OmniboxEventProto::OTHER,
                        TestSchemeClassifier());

  SetAutocompleteMatches(
      {CreateSearchMatch(u"Clear History"),
       CreateHistoryURLMatch(
           /*destination_url=*/"http://this-site-matches.com")});

  controller_.AttachActions();

  // For any other page classification, AttachActions should attach a switch
  // to this tab action to the relevant matches.
  EXPECT_TRUE(
      controller_.internal_result_.match_at(1)->has_tab_match.value_or(false));
}

TEST_F(AutocompleteControllerTest, UpdateAssociatedKeywords) {
  controller_.keyword_provider_ =
      new KeywordProvider(provider_client(), nullptr);
  controller_.providers_.push_back(controller_.keyword_provider_.get());

  auto add_keyword = [&](std::u16string keyword, bool is_starter_pack = false,
                         bool is_featured_enterprise_search = false) {
    TemplateURLData turl_data;
    turl_data.SetShortName(u"name");
    turl_data.SetURL("https://google.com/search?q={searchTerms}");
    turl_data.is_active = TemplateURLData::ActiveStatus::kTrue;
    turl_data.SetKeyword(keyword);
    if (is_starter_pack) {
      turl_data.starter_pack_id = 1;
    } else if (is_featured_enterprise_search) {
      turl_data.featured_by_policy = true;
    }
    controller_.template_url_service_->Add(
        std::make_unique<TemplateURL>(turl_data));
  };

  struct MatchData {
    std::u16string fill_into_edit;
    AutocompleteMatchType::Type type;
  };

  auto test = [&](const std::u16string input_text,
                  const std::u16string input_keyword,
                  std::vector<MatchData> match_datas,
                  bool is_zero_suggest = false) {
    controller_.input_ = FakeAutocompleteController::CreateInput(input_text);
    if (is_zero_suggest) {
      controller_.input_.set_focus_type(
          metrics::OmniboxFocusType::INTERACTION_FOCUS);
    }
    AutocompleteResult result;
    for (const auto& match_data : match_datas) {
      AutocompleteMatch match;
      match.fill_into_edit = match_data.fill_into_edit;
      match.type = match_data.type;
      result.AppendMatches({match});
    }
    if (!input_keyword.empty()) {
      result.match_at(0)->keyword = input_keyword;
      result.match_at(0)->transition = ui::PAGE_TRANSITION_KEYWORD;
    }
    controller_.UpdateAssociatedKeywords(&result);

    std::vector<std::u16string> attached_keywords;
    for (const auto& match : result) {
      attached_keywords.push_back(match.associated_keyword);
    }
    return attached_keywords;
  };

  add_keyword(u"keyword_0");
  add_keyword(u"keyword_1");
  add_keyword(u"keyword_starter_pack", true);
  add_keyword(u"keyword_featured_enterprise_search", false, true);

  // When the input text's 1st word matches a keyword, the keyword hint is added
  // to the 1st match regardless of which match is similar to the keyword. Only
  // 1 keyword is added even if there's another match matching the keyword.
  EXPECT_THAT(test(u"keyword_0", u"", {{u"bing.com"}, {u"keyword_0"}}),
              testing::ElementsAreArray({u"keyword_0", u""}));
  EXPECT_THAT(
      test(u"keyword_0 more words", u"", {{u"bing.com"}, {u"keyword_0"}}),
      testing::ElementsAreArray({u"keyword_0", u""}));

  // Only 1 keyword is added even if there're 2 non-exact matches matching the
  // keyword.
  EXPECT_THAT(
      test(u"input", u"", {{u"bing.com"}, {u"keyword_0"}, {u"keyword_0"}}),
      testing::ElementsAreArray({u"", u"keyword_0", u""}));

  // When the user is in a keyword mode, don't show keyword hints for that
  // keyword, but do still show other keywords.
  EXPECT_THAT(test(u"keyword_0", u"keyword_0",
                   {{u"keyword_0"}, {u"keyword_0"}, {u"keyword_1"}}),
              testing::ElementsAreArray({u"", u"", u"keyword_1"}));

  // Starter pack and featured enterprise matches should always have keywords,
  // regardless of the input or the match position.
  EXPECT_THAT(test(u"input", u"",
                   {{u"keyword_starter_pack",
                     AutocompleteMatchType::Type::STARTER_PACK},
                    {u"keyword_featured_enterprise_search",
                     AutocompleteMatchType::Type::FEATURED_ENTERPRISE_SEARCH},
                    {u"keyword_0"},
                    {u"keywo"}}),
              testing::ElementsAreArray({u"keyword_starter_pack",
                                         u"keyword_featured_enterprise_search",
                                         u"keyword_0", u""}));

  // Normal matches should not have keyword hints for starter pack or featured
  // enterprise keywords.
  EXPECT_THAT(test(u"input", u"",
                   {{u"keyword_starter_pack"},
                    {u"keyword_featured_enterprise_search"}}),
              testing::ElementsAreArray({u"", u""}));

  // Normal matches should not have keyword hints for starter pack or featured
  // enterprise keywords, even if the input is an exact keyword match.
  EXPECT_THAT(test(u"keyword_starter_pack", u"", {{u"keyword_starter_pack"}}),
              testing::ElementsAreArray({u""}));
  EXPECT_THAT(test(u"keyword_featured_enterprise_search", u"",
                   {{u"keyword_featured_enterprise_search"}}),
              testing::ElementsAreArray({u""}));

  // Keywords are added if the 1st word of the match text matches, even if the
  // match text has more non-matching words after. Keywords are not added if the
  // 1st word of the match text is a prefix of or prefixed by the keyword.
  EXPECT_THAT(
      test(u"input", u"",
           {{u"keywo"}, {u"keyword_0_underscore"}, {u"keyword_0 space"}}),
      testing::ElementsAreArray({u"", u"", u"keyword_0"}));

  EXPECT_THAT(test(u"", u"",
                   {{u"keywo", AutocompleteMatchType::Type::NAVSUGGEST},
                    {u"keyword_0_underscore"},
                    {u"keyword_0 space"}},
                   /*is_zero_suggest=*/true),
              testing::ElementsAreArray({u"", u"", u""}));
}

// Helper function to create a basic AutocompleteMatch for testing default match
// changes.
AutocompleteMatch CreateDefaultMatch(std::u16string fill_into_edit,
                                     GURL icon_url,
                                     std::u16string associated_keyword,
                                     std::u16string keyword) {
  AutocompleteMatch match;
  match.fill_into_edit = fill_into_edit;
  match.icon_url = icon_url;
  match.associated_keyword = associated_keyword;
  match.keyword = keyword;

  // Set other fields to make it a plausible default match
  match.relevance = 1000;
  match.allowed_to_be_default_match = true;
  match.destination_url =
      GURL("https://foo.com/" + base::UTF16ToUTF8(match.fill_into_edit));

  return match;
}

TEST_F(AutocompleteControllerTest, CheckWhetherDefaultMatchChanged) {
  // Helper lambda to set the internal default match
  auto set_current_default = [&](std::optional<AutocompleteMatch> match) {
    controller_.internal_result_.ClearMatches();  // Clear previous matches
    if (match) {
      controller_.internal_result_.AppendMatches({*match});
    }
  };

  // Helper lambda to call the private method under test
  auto check_change =
      [&](std::optional<AutocompleteMatch> last_default_match,
          const std::u16string& last_default_associated_keyword) {
        // Reset timestamp before check
        controller_.last_time_default_match_changed_ = base::TimeTicks();
        bool changed = controller_.CheckWhetherDefaultMatchChanged(
            last_default_match, last_default_associated_keyword);
        // Check if timestamp was updated only if a change was detected
        if (changed) {
          EXPECT_NE(controller_.last_time_default_match_changed_,
                    base::TimeTicks());
        } else {
          EXPECT_EQ(controller_.last_time_default_match_changed_,
                    base::TimeTicks());
        }
        return changed;
      };

  {
    // No change: Both null
    set_current_default(std::nullopt);
    EXPECT_FALSE(check_change(std::nullopt, u""));
  }
  {
    // No change: Both exist and are identical
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    set_current_default(match);
    EXPECT_FALSE(check_change(match, u"assoc1"));
  }
  {
    // No change: Irrelevant fields differ (e.g., relevance)
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_different_relevance = match;
    match_different_relevance.relevance = 900;
    set_current_default(match);
    EXPECT_FALSE(check_change(match_different_relevance, u"assoc1"));
  }
  {
    // Change: Existence (last had value, current doesn't)
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    set_current_default(std::nullopt);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // Change: Existence (last didn't have value, current does)
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    set_current_default(match);
    EXPECT_TRUE(check_change(std::nullopt, u""));
  }
  {
    // Change: fill_into_edit differs
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_different_fill_into_edit = CreateDefaultMatch(
        u"test2", GURL("https://www.foo.com/icon1"), u"assoc1", u"key1");
    set_current_default(match_different_fill_into_edit);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // Change: icon_url differs
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_different_icon_url = CreateDefaultMatch(
        u"test1", GURL("https://www.foo.com/icon2"), u"assoc1", u"key1");
    set_current_default(match_different_icon_url);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // Change: associated_keyword existence differs (last had, current doesn't)
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_no_associated_keyword = CreateDefaultMatch(
        u"test1", GURL("https://www.foo.com/icon1"), u"", u"key1");
    set_current_default(match_no_associated_keyword);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // Change: associated_keyword existence differs (last didn't, current does)
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_no_associated_keyword = CreateDefaultMatch(
        u"test1", GURL("https://www.foo.com/icon1"), u"", u"key1");
    set_current_default(match);
    EXPECT_TRUE(
        check_change(match_no_associated_keyword, u""));  // double check this
  }
  {
    // Change: associated_keyword differs
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_different_associated_keyword = CreateDefaultMatch(
        u"test1", GURL("https://www.foo.com/icon1"), u"assoc2", u"key1");
    set_current_default(match_different_associated_keyword);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // No change: associated_keyword same
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    set_current_default(match);
    EXPECT_FALSE(check_change(match, u"assoc1"));
  }
  {
    // Change: keyword differs
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_different_keyword = CreateDefaultMatch(
        u"test1", GURL("https://www.foo.com/icon1"), u"assoc1", u"key2");
    set_current_default(match_different_keyword);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // No change: keyword same
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    set_current_default(match);
    EXPECT_FALSE(check_change(match, u"assoc1"));
  }
}

TEST_F(AutocompleteControllerTest, SmartComposeClearedWithNewResults) {
  auto match1 = CreateSearchMatch("match1", true, 1300);
  EXPECT_THAT(controller_.SimulateAutocompletePass(true, false, {match1}),
              testing::ElementsAreArray({
                  "match1",
              }));

  controller_.internal_result_.set_smart_compose_inline_hint("smart compose!");

  // Verify smart compose field is set initially
  ASSERT_TRUE(
      !controller_.internal_result_.smart_compose_inline_hint().empty());

  EXPECT_THAT(controller_.SimulateAutocompletePass(true, false, {match1}),
              testing::ElementsAreArray({
                  "match1",
              }));

  // Smart compose field should not be set after autocomplete pass with no
  // smart compose result.
  ASSERT_TRUE(controller_.internal_result_.smart_compose_inline_hint().empty());
}

TEST_F(AutocompleteControllerTest, IncludesSmartComposeStatsInAdditionalStats) {
  omnibox::metrics::SmartComposeStats stats;
  stats.set_enabled(true);
  stats.set_shown_count(1);
  stats.set_accepted_count(2);
  stats.set_characters_accepted(3);
  stats.set_shown_length(4);
  controller_.SetSmartComposeStats(stats);

  TemplateURLRef::SearchTermsArgs search_terms_args(u"test");
  controller_.UpdateSearchTermsArgsWithAdditionalSearchboxStats(
      base::TimeDelta(), search_terms_args);

  ASSERT_TRUE(search_terms_args.searchbox_stats.has_smart_compose_stats());
  EXPECT_TRUE(
      search_terms_args.searchbox_stats.smart_compose_stats().enabled());
  EXPECT_EQ(
      search_terms_args.searchbox_stats.smart_compose_stats().shown_count(), 1);
  EXPECT_EQ(
      search_terms_args.searchbox_stats.smart_compose_stats().accepted_count(),
      2);
  EXPECT_EQ(search_terms_args.searchbox_stats.smart_compose_stats()
                .characters_accepted(),
            3);
  EXPECT_EQ(
      search_terms_args.searchbox_stats.smart_compose_stats().shown_length(),
      4);
}

TEST_F(AutocompleteControllerTest, PersistsExperimentStatsV2InSession) {
  // Inject zero suggest provider to supply fake experiment stats.
  scoped_refptr<ZeroSuggestProvider> zero_suggest_provider =
      base::MakeRefCounted<ZeroSuggestProvider>(provider_client(),
                                                &controller_);
  controller_.providers_.push_back(zero_suggest_provider);
  controller_.zero_suggest_provider_ = zero_suggest_provider.get();

  omnibox::metrics::ChromeSearchboxStats::ExperimentStatsV2 stat;
  stat.set_type_int(12345);
  stat.set_string_value("dummy:stat");
  const_cast<SearchSuggestionParser::ExperimentStatsV2s&>(
      zero_suggest_provider->experiment_stats_v2s())
      .push_back(stat);

  // Set initial matches (simulating zero-suggest run).
  TemplateURLData turl_data;
  turl_data.SetShortName(u"Search");
  turl_data.SetKeyword(u"search");
  turl_data.SetURL("https://google.com/search?q={searchTerms}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(turl_data));
  AutocompleteMatch match(nullptr, 1100, false,
                          AutocompleteMatchType::SEARCH_SUGGEST);
  match.keyword = u"search";
  match.destination_url = GURL("https://google.com/search?q=foo");
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(u"foo");
  SetAutocompleteMatches({match});

  // This should populate session-level experiment stats and searchbox_stats.
  UpdateSearchboxStats();

  ASSERT_EQ(1U,
            controller_.internal_result_.session().experiment_stats_v2s.size());
  EXPECT_EQ(12345, controller_.internal_result_.session()
                       .experiment_stats_v2s[0]
                       .type_int());
  EXPECT_EQ("dummy,stat", controller_.internal_result_.session()
                              .experiment_stats_v2s[0]
                              .string_value());

  // Run another pass while the provider is still active, to verify that
  // duplicates are not added to the session data.
  UpdateSearchboxStats();
  ASSERT_EQ(1U,
            controller_.internal_result_.session().experiment_stats_v2s.size());

  // Now, simulate the user typing (which clobbers/stops zero suggest provider).
  zero_suggest_provider->Stop(AutocompleteStopReason::kClobbered);
  ASSERT_TRUE(zero_suggest_provider->experiment_stats_v2s().empty());

  // Run another autocomplete pass (e.g. typing query "foo").
  AutocompleteMatch typed_match(nullptr, 1100, false,
                                AutocompleteMatchType::SEARCH_SUGGEST);
  typed_match.keyword = u"search";
  typed_match.destination_url = GURL("https://google.com/search?q=foo");
  typed_match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(u"foo");
  SetAutocompleteMatches({typed_match});

  UpdateSearchboxStats();

  // The session data should still persist the stats.
  ASSERT_EQ(1U,
            controller_.internal_result_.session().experiment_stats_v2s.size());

  // And the final searchbox_stats of the match should contain the persisted
  // stats.
  const auto& stats = controller_.internal_result_.match_at(0)
                          ->search_terms_args->searchbox_stats;
  ASSERT_EQ(1, stats.experiment_stats_v2_size());
  EXPECT_EQ(12345, stats.experiment_stats_v2(0).type_int());
  EXPECT_EQ("dummy,stat", stats.experiment_stats_v2(0).string_value());
}

TEST_F(AutocompleteControllerTest,
       MaybeProcessInlineLocationSuggestionMatch_ResetsPermission) {
  AutocompleteMatch match(nullptr, 1100, false,
                          AutocompleteMatchType::SEARCH_SUGGEST);
  match.subtypes.insert(omnibox::SUBTYPE_LOCATION_SUGGEST_TRIGGER);
  match.extra_headers[kXGeoHeader] = "w test";
  match.destination_url = GURL("https://www.google.com/search?q=coffee");

  controller_.MaybeProcessInlineLocationSuggestionMatch(match);

  EXPECT_EQ(1, provider_client()->reset_geolocation_call_count());
  EXPECT_EQ(match.destination_url,
            provider_client()->last_reset_geolocation_url());
}
