// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_provider_test.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_text_checking_result.h"
#include "third_party/blink/public/web/web_text_decoration_type.h"

namespace {

using testing::IsEmpty;

class SpellCheckProviderCacheTest : public SpellCheckProviderTest {
 protected:
  void UpdateCustomDictionary() {
    SpellCheck* spellcheck = provider_.spellcheck();
    EXPECT_NE(spellcheck, nullptr);
    // Skip adding friend class - use public CustomDictionaryChanged from
    // |spellcheck::mojom::SpellChecker|
    static_cast<spellcheck::mojom::SpellChecker*>(spellcheck)
        ->CustomDictionaryChanged({}, {});
  }
};

TEST_F(SpellCheckProviderCacheTest, SubstringWithoutMisspellings) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  std::vector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(u"This is a test", last_results);
  EXPECT_TRUE(provider_.SatisfyRequestFromCache(u"This is a", &completion));
  EXPECT_EQ(result.completion_count_, 1U);
}

TEST_F(SpellCheckProviderCacheTest, SubstringWithMisspellings) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  std::vector<blink::WebTextCheckingResult> last_results = {
      blink::WebTextCheckingResult(blink::kWebTextDecorationTypeSpelling, 5, 3,
                                   std::vector<blink::WebString>({"isq"}))};
  provider_.SetLastResults(u"This isq a test", last_results);
  EXPECT_TRUE(provider_.SatisfyRequestFromCache(u"This isq a", &completion));
  EXPECT_EQ(result.completion_count_, 1U);
}

TEST_F(SpellCheckProviderCacheTest, ShorterTextNotSubstring) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  std::vector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(u"This is a test", last_results);
  EXPECT_FALSE(provider_.SatisfyRequestFromCache(u"That is a", &completion));
  EXPECT_EQ(result.completion_count_, 0U);
}

TEST_F(SpellCheckProviderCacheTest, ResetCacheOnCustomDictionaryUpdate) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  std::vector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(u"This is a test", last_results);

  UpdateCustomDictionary();

  EXPECT_FALSE(provider_.SatisfyRequestFromCache(u"This is a", &completion));
  EXPECT_EQ(result.completion_count_, 0U);
}

TEST_F(SpellCheckProviderCacheTest,
       ResetCacheOnSpellCheckCustomDictionaryUpdate) {
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "SpellCheckCustomDictionaryAPI", true);

  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  std::vector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(u"This is a test", last_results);

  SpellCheck* spellcheck = provider_.spellcheck();
  EXPECT_NE(spellcheck, nullptr);

  provider_.spellcheck()->InitializeSpellCheckWithLanguage();
  static_cast<blink::WebTextCheckClient*>(&provider_)
      ->SpellCheckCustomDictionaryChanged({}, {});

  EXPECT_FALSE(provider_.SatisfyRequestFromCache(u"This is a", &completion));
  EXPECT_EQ(result.completion_count_, 0U);
}

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
// Verifies that a word added through the SpellCheckCustomDictionary web API is
// dropped from results returned by the browser-side platform spell checker
// (NSSpellChecker on macOS). Without the post-filter, the API would have no
// effect on these platforms because the platform spell checker has no
// knowledge of the per-document custom word set.
TEST_F(SpellCheckProviderTest, DocumentCustomDictionaryFiltersPlatformResults) {
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "SpellCheckCustomDictionaryAPI", true);

  // Push a custom word through the per-frame web API entry point.
  static_cast<blink::WebTextCheckClient*>(&provider_)
      ->SpellCheckCustomDictionaryChanged({"Pikachu"}, {});

  FakeTextCheckingResult completion_result;
  provider_.RequestTextChecking(
      u"i love Pikachu", /*spelling_markers=*/{},
      blink::WebTextCheckClient::ShouldForceRefreshTextCheckService::kNo,
      std::make_unique<FakeTextCheckingCompletion>(&completion_result));

  // The fake SpellCheckHost recorded the platform-spellcheck mojo request.
  ASSERT_EQ(provider_.text_check_requests_.size(), 1u);

  // Simulate the platform spell checker reporting "Pikachu" (offset 7,
  // length 7 in "i love Pikachu") as a misspelling.
  std::vector<SpellCheckResult> platform_results = {
      SpellCheckResult(spellcheck::Decoration::SPELLING, /*loc=*/7, /*len=*/7)};
  std::move(std::get<2>(provider_.text_check_requests_.back()))
      .Run(platform_results);

  // The mojo response callback posts back to this thread; wait for the
  // completion to fire before inspecting it.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return completion_result.completion_count_ > 0; }));

  // The post-filter dropped "Pikachu", so Blink sees no misspellings.
  EXPECT_EQ(completion_result.completion_count_, 1u);
  EXPECT_EQ(completion_result.cancellation_count_, 0u);
  EXPECT_THAT(completion_result.results_, IsEmpty());
}

// RTL analog of the test above: a right-to-left (Hebrew) custom word added
// through the web API must also suppress a platform-reported misspelling.
TEST_F(SpellCheckProviderTest,
       DocumentCustomDictionaryFiltersPlatformResultsRtl) {
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "SpellCheckCustomDictionaryAPI", true);

  // "שלוס" (U+05E9 U+05DC U+05D5 U+05E1), a Hebrew nonsense word; each letter
  // is 2 bytes in UTF-8. The u"" text below spells the same code points.
  const std::string kHebrewUtf8 = "\xD7\xA9\xD7\x9C\xD7\x95\xD7\xA1";
  const std::u16string kHebrewUtf16 = u"שלוס";
  static_cast<blink::WebTextCheckClient*>(&provider_)
      ->SpellCheckCustomDictionaryChanged({kHebrewUtf8}, {});

  const std::u16string text = u"hi " + kHebrewUtf16;
  const size_t loc = text.find(kHebrewUtf16);
  ASSERT_NE(loc, std::u16string::npos);
  const size_t len = kHebrewUtf16.size();

  FakeTextCheckingResult completion_result;
  provider_.RequestTextChecking(
      text, /*spelling_markers=*/{},
      blink::WebTextCheckClient::ShouldForceRefreshTextCheckService::kNo,
      std::make_unique<FakeTextCheckingCompletion>(&completion_result));

  ASSERT_EQ(provider_.text_check_requests_.size(), 1u);

  // Simulate the platform spell checker flagging the Hebrew word.
  std::vector<SpellCheckResult> platform_results = {
      SpellCheckResult(spellcheck::Decoration::SPELLING, static_cast<int>(loc),
                       static_cast<int>(len))};
  std::move(std::get<2>(provider_.text_check_requests_.back()))
      .Run(platform_results);

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return completion_result.completion_count_ > 0; }));

  // The post-filter recognized the RTL custom word and dropped it.
  EXPECT_EQ(completion_result.completion_count_, 1u);
  EXPECT_EQ(completion_result.cancellation_count_, 0u);
  EXPECT_THAT(completion_result.results_, IsEmpty());
}

// Verifies that committing a new document drops the per-frame custom word
// set, so a word added before the navigation no longer suppresses platform
// misspellings reported for the next document.
TEST_F(SpellCheckProviderTest,
       DidCreateNewDocumentClearsDocumentCustomDictionary) {
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "SpellCheckCustomDictionaryAPI", true);

  // Push a custom word, then simulate the RenderFrame creating a new document.
  static_cast<blink::WebTextCheckClient*>(&provider_)
      ->SpellCheckCustomDictionaryChanged({"Pikachu"}, {});
  provider_.DidCreateNewDocument();

  FakeTextCheckingResult completion_result;
  provider_.RequestTextChecking(
      u"i love Pikachu", /*spelling_markers=*/{},
      blink::WebTextCheckClient::ShouldForceRefreshTextCheckService::kNo,
      std::make_unique<FakeTextCheckingCompletion>(&completion_result));

  ASSERT_EQ(provider_.text_check_requests_.size(), 1u);

  // Platform spell checker flags "Pikachu" as misspelled.
  std::vector<SpellCheckResult> platform_results = {
      SpellCheckResult(spellcheck::Decoration::SPELLING, /*loc=*/7, /*len=*/7)};
  std::move(std::get<2>(provider_.text_check_requests_.back()))
      .Run(platform_results);

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return completion_result.completion_count_ > 0; }));

  // The custom word set was cleared on the new document, so the filter no
  // longer suppresses "Pikachu" and the misspelling reaches Blink.
  EXPECT_EQ(completion_result.completion_count_, 1u);
  EXPECT_EQ(completion_result.cancellation_count_, 0u);
  EXPECT_EQ(completion_result.results_.size(), 1u);
}

#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
// Verifies that a word added through the SpellCheckCustomDictionary web API is
// dropped from results returned via the enhanced spelling service path.
TEST_F(SpellCheckProviderTest,
       DocumentCustomDictionaryFiltersSpellingServiceResults) {
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "SpellCheckCustomDictionaryAPI", true);

  // Push a custom word through the per-frame web API entry point.
  static_cast<blink::WebTextCheckClient*>(&provider_)
      ->SpellCheckCustomDictionaryChanged({"Pikachu"}, {});

  FakeTextCheckingResult completion_result;
  int check_id = provider_.AddCompletionForTest(
      std::make_unique<FakeTextCheckingCompletion>(&completion_result));

  // Simulate the enhanced spelling service reporting "Pikachu" (offset 7,
  // length 7 in "i love Pikachu") as a misspelling.
  std::vector<SpellCheckResult> service_results = {
      SpellCheckResult(spellcheck::Decoration::SPELLING, /*loc=*/7, /*len=*/7)};
  provider_.OnRespondSpellingService(check_id, u"i love Pikachu",
                                     /*success=*/true, service_results);

  // The post-filter dropped "Pikachu", so Blink sees no misspellings.
  EXPECT_EQ(completion_result.completion_count_, 1u);
  EXPECT_EQ(completion_result.cancellation_count_, 0u);
  EXPECT_TRUE(completion_result.results_.empty());
}

// Companion to the test above: the post-filter must be selective. A word that
// is NOT in the per-document custom dictionary should still be reported as a
// misspelling.
TEST_F(SpellCheckProviderTest,
       DocumentCustomDictionaryKeepsNonCustomSpellingServiceResults) {
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "SpellCheckCustomDictionaryAPI", true);

  // Only "Pikachu" is in the document custom dictionary.
  static_cast<blink::WebTextCheckClient*>(&provider_)
      ->SpellCheckCustomDictionaryChanged({"Pikachu"}, {});

  FakeTextCheckingResult completion_result;
  int check_id = provider_.AddCompletionForTest(
      std::make_unique<FakeTextCheckingCompletion>(&completion_result));

  // The enhanced spelling service flags both words.
  std::vector<SpellCheckResult> service_results = {
      SpellCheckResult(spellcheck::Decoration::SPELLING, /*loc=*/0, /*len=*/7),
      SpellCheckResult(spellcheck::Decoration::SPELLING, /*loc=*/8, /*len=*/8)};
  provider_.OnRespondSpellingService(check_id, u"Pikachu Squirtle",
                                     /*success=*/true, service_results);

  // "Pikachu" was dropped, but "Squirtle" survives as a misspelling.
  EXPECT_EQ(completion_result.completion_count_, 1u);
  EXPECT_EQ(completion_result.cancellation_count_, 0u);
  ASSERT_EQ(completion_result.results_.size(), 1u);
  EXPECT_EQ(completion_result.results_[0].location, 8);
  EXPECT_EQ(completion_result.results_[0].length, 8);
}
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

// Verifies the WordCount histogram is emitted with the count of accepted API
// additions for the outgoing document when a new document is committed.
TEST_F(SpellCheckProviderTest, DocumentCustomDictionaryRecordsWordCountUma) {
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "SpellCheckCustomDictionaryAPI", true);

  base::HistogramTester histograms;
  static_cast<blink::WebTextCheckClient*>(&provider_)
      ->SpellCheckCustomDictionaryChanged({"alpha", "beta", "gamma"}, {});

  // Nothing recorded yet — the document is still alive.
  histograms.ExpectTotalCount("Spellcheck.DocumentCustomDictionary.WordCount",
                              0);

  // Committing a new document records the outgoing count.
  provider_.DidCreateNewDocument();
  histograms.ExpectUniqueSample("Spellcheck.DocumentCustomDictionary.WordCount",
                                3, 1);

  // A subsequent empty document does not add another sample.
  provider_.DidCreateNewDocument();
  histograms.ExpectTotalCount("Spellcheck.DocumentCustomDictionary.WordCount",
                              1);
}

// Verifies removals shrink the live per-document set on every build: the cap
// tracks the resident set rather than lifetime additions, so removing words
// later frees room and the recorded WordCount reflects adds minus removals.
TEST_F(SpellCheckProviderTest, DocumentCustomDictionaryRemovalDecrementsCount) {
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "SpellCheckCustomDictionaryAPI", true);

  base::HistogramTester histograms;
  auto* client = static_cast<blink::WebTextCheckClient*>(&provider_);
  client->SpellCheckCustomDictionaryChanged({"alpha", "beta", "gamma"}, {});
  client->SpellCheckCustomDictionaryChanged(
      /*words_added=*/{},
      /*words_removed=*/{"alpha", "beta"});

  // Two of the three words were removed, leaving one in the live set. Under a
  // monotonic counter this would record 3.
  provider_.DidCreateNewDocument();
  histograms.ExpectUniqueSample("Spellcheck.DocumentCustomDictionary.WordCount",
                                1, 1);
}

// Verifies that additions beyond kMaxDocumentCustomDictionaryWords are dropped
// on every platform.
TEST_F(SpellCheckProviderTest, DocumentCustomDictionaryEnforcesWordCountCap) {
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "SpellCheckCustomDictionaryAPI", true);

  // Supply one word beyond the cap; the last one is expected to be dropped.
  std::vector<std::string> words;
  words.reserve(spellcheck::kMaxDocumentCustomDictionaryWords + 1);
  for (size_t i = 0; i < spellcheck::kMaxDocumentCustomDictionaryWords + 1;
       ++i) {
    words.push_back("w" + base::NumberToString(i));
  }

  base::HistogramTester histograms;
  static_cast<blink::WebTextCheckClient*>(&provider_)
      ->SpellCheckCustomDictionaryChanged(words, {});

  // Committing the document records the live set size, which is capped at the
  // limit rather than the number of words supplied.
  provider_.DidCreateNewDocument();
  histograms.ExpectUniqueSample("Spellcheck.DocumentCustomDictionary.WordCount",
                                spellcheck::kMaxDocumentCustomDictionaryWords,
                                1);
}

// Verifies that a word exceeding kMaxDocumentCustomDictionaryWordBytes is
// dropped on insert even when there is room in the per-document set.
TEST_F(SpellCheckProviderTest, DocumentCustomDictionaryRejectsOverlongWord) {
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "SpellCheckCustomDictionaryAPI", true);

  const std::string overlong(
      spellcheck::kMaxDocumentCustomDictionaryWordBytes + 1, 'a');
  base::HistogramTester histograms;
  // The overlong word is dropped; the well-formed word is accepted.
  static_cast<blink::WebTextCheckClient*>(&provider_)
      ->SpellCheckCustomDictionaryChanged({overlong, "valid"}, {});

  // Only the well-formed word remains in the live set.
  provider_.DidCreateNewDocument();
  histograms.ExpectUniqueSample("Spellcheck.DocumentCustomDictionary.WordCount",
                                1, 1);
}

}  // namespace
