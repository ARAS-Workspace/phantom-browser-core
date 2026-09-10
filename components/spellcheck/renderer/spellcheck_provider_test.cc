// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_provider_test.h"

#include <memory>

#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/common/spelling_marker.h"
#include "components/spellcheck/renderer/hunspell_engine.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_language.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_text_check_client.h"

FakeTextCheckingResult::FakeTextCheckingResult() = default;
FakeTextCheckingResult::~FakeTextCheckingResult() = default;

FakeTextCheckingCompletion::FakeTextCheckingCompletion(
    FakeTextCheckingResult* result)
    : result_(result) {}

FakeTextCheckingCompletion::~FakeTextCheckingCompletion() = default;

void FakeTextCheckingCompletion::DidFinishCheckingText(
    const std::vector<blink::WebTextCheckingResult>& results) {
  ++result_->completion_count_;
  result_->results_ = results;
}

void FakeTextCheckingCompletion::DidCancelCheckingText() {
  ++result_->completion_count_;
  ++result_->cancellation_count_;
}

FakeSpellCheck::FakeSpellCheck(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : SpellCheck(embedder_provider) {}

void FakeSpellCheck::SetFakeLanguageCounts(size_t language_count,
                                           size_t enabled_count) {
  use_fake_counts_ = true;
  language_count_ = language_count;
  enabled_language_count_ = enabled_count;
}

void FakeSpellCheck::InitializeSpellCheckWithLanguage() {
  // Add the SpellcheckLanguage manually to the SpellCheck object.
  SpellCheck::languages_.push_back(
      std::make_unique<SpellcheckLanguage>(embedder_provider_));
}

size_t FakeSpellCheck::LanguageCount() {
  return use_fake_counts_ ? language_count_ : SpellCheck::LanguageCount();
}

size_t FakeSpellCheck::EnabledLanguageCount() {
  return use_fake_counts_ ? enabled_language_count_
                          : SpellCheck::EnabledLanguageCount();
}

TestingSpellCheckProvider::TestingSpellCheckProvider(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : SpellCheckProvider(nullptr, new FakeSpellCheck(embedder_provider)) {
  SetSpellCheckHostForTesting(receiver_.BindNewPipeAndPassRemote());
}

TestingSpellCheckProvider::TestingSpellCheckProvider(
    SpellCheck* spellcheck,
    service_manager::LocalInterfaceProvider* embedder_provider)
    : SpellCheckProvider(nullptr, spellcheck) {
  SetSpellCheckHostForTesting(receiver_.BindNewPipeAndPassRemote());
}

TestingSpellCheckProvider::~TestingSpellCheckProvider() {
  receiver_.reset();
  // dictionary_update_observer_ must be released before deleting spellcheck_.
  ResetDictionaryUpdateObserverForTesting();
  delete spellcheck_;
}

void TestingSpellCheckProvider::RequestTextChecking(
    const std::u16string& text,
    const std::vector<spellcheck::SpellingMarker>& spelling_markers,
    blink::WebTextCheckClient::ShouldForceRefreshTextCheckService
        should_force_refresh,
    std::unique_ptr<blink::WebTextCheckingCompletion> completion) {
  SpellCheckProvider::RequestTextChecking(
      text, spelling_markers, should_force_refresh, std::move(completion));
  base::RunLoop().RunUntilIdle();
}

void TestingSpellCheckProvider::NotifyChecked(const std::u16string& word,
                                              bool misspelled) {}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void TestingSpellCheckProvider::CallSpellingService(
    const std::u16string& text,
    CallSpellingServiceCallback callback) {
  OnCallSpellingService(text);
  std::move(callback).Run(true, std::vector<SpellCheckResult>());
}

void TestingSpellCheckProvider::OnCallSpellingService(
    const std::u16string& text) {
  ++spelling_service_call_count_;
  if (!text_check_completions_.Lookup(last_identifier_)) {
    ResetResult();
    return;
  }
  text_.assign(text);
  std::unique_ptr<blink::WebTextCheckingCompletion> completion(
      text_check_completions_.Replace(last_identifier_, nullptr));
  text_check_completions_.Remove(last_identifier_);
  std::vector<blink::WebTextCheckingResult> results;
  results.push_back(
      blink::WebTextCheckingResult(blink::kWebTextDecorationTypeSpelling, 0, 5,
                                   std::vector<blink::WebString>({"hello"})));
  completion->DidFinishCheckingText(results);
  last_request_ = text;
  last_results_ = results;
}

void TestingSpellCheckProvider::ResetResult() {
  text_.clear();
}

int TestingSpellCheckProvider::AddCompletionForTest(
    std::unique_ptr<FakeTextCheckingCompletion> completion) {
  return SpellCheckProvider::text_check_completions_.Add(std::move(completion));
}

void TestingSpellCheckProvider::OnRespondSpellingService(
    int identifier,
    const std::u16string& text,
    bool success,
    const std::vector<SpellCheckResult>& results) {
  SpellCheckProvider::OnRespondSpellingService(identifier, text, success,
                                               results);
}
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void TestingSpellCheckProvider::RequestTextCheck(
    const std::u16string& text,
    const std::vector<spellcheck::SpellingMarker>& spelling_markers,
    RequestTextCheckCallback callback) {
  text_check_requests_.emplace_back(text, spelling_markers,
                                    std::move(callback));
}

#if BUILDFLAG(ENABLE_SPELLING_SERVICE)
void TestingSpellCheckProvider::CheckSpelling(const std::u16string&,
                                              CheckSpellingCallback) {
  NOTREACHED();
}

void TestingSpellCheckProvider::FillSuggestionList(const std::u16string&,
                                                   FillSuggestionListCallback) {
  NOTREACHED();
}
#endif  // BUILDFLAG(ENABLE_SPELLING_SERVICE)

#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(IS_ANDROID)
void TestingSpellCheckProvider::DisconnectSessionBridge() {
  NOTREACHED();
}
#endif

void TestingSpellCheckProvider::SetLastResults(
    const std::u16string last_request,
    std::vector<blink::WebTextCheckingResult>& last_results) {
  last_request_ = last_request;
  last_results_ = last_results;
}

bool TestingSpellCheckProvider::SatisfyRequestFromCache(
    const std::u16string& text,
    blink::WebTextCheckingCompletion* completion) {
  return SpellCheckProvider::SatisfyRequestFromCache(text, completion);
}

base::WeakPtr<SpellCheckProvider> TestingSpellCheckProvider::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

SpellCheckProviderTest::SpellCheckProviderTest()
    : provider_(&embedder_provider_) {}
SpellCheckProviderTest::~SpellCheckProviderTest() = default;

void SpellCheckProviderTest::SetUp() {
  custom_dictionary_api_enabled_ =
      blink::WebRuntimeFeatures::IsSpellCheckCustomDictionaryAPIEnabled();
}

void SpellCheckProviderTest::TearDown() {
  blink::WebRuntimeFeatures::EnableSpellCheckCustomDictionaryAPI(
      custom_dictionary_api_enabled_);
}
