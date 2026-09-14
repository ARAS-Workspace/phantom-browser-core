// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_language_list.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/common/locale_util.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_event_details.h"
#include "components/translate/core/browser/translate_url_fetcher.h"
#include "components/translate/core/browser/translate_url_util.h"
#include "components/translate/core/common/translate_language_matcher.h"
#include "components/translate/core/common/translate_util.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace translate {
namespace {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::GetLanguageTagFromString;
using ::base::i18n::LanguageTag;

// Constant URL string to fetch server supporting language list.
constexpr std::string_view kLanguageListFetchPath =
    "translate_a/l?client=chrome";

// Retry parameter for fetching.
constexpr int kMaxRetryOn5xx = 5;

void SortAndUnique(std::vector<LanguageTag>& languages) {
  std::sort(languages.begin(), languages.end());
  languages.erase(std::unique(languages.begin(), languages.end()),
                  languages.end());
}

}  // namespace

const char TranslateLanguageList::kTargetLanguagesKey[] = "tl";

TranslateLanguageList::TranslateLanguageList()
    : TranslateLanguageList(
          std::make_unique<TranslateURLFetcherImpl>(kMaxRetryOn5xx)) {}

TranslateLanguageList::TranslateLanguageList(
    std::unique_ptr<TranslateUrlFetcher> fetcher)
    : resource_requests_allowed_(false),
      request_pending_(false),
      // We default to our hard coded list of languages in
      // |translate::GetDefaultSupportedLanguages()|. This list will be
      // overridden by a server providing supported languages list.
      supported_languages_(std::from_range,
                           translate::GetDefaultSupportedLanguages()),
      language_list_fetcher_(std::move(fetcher)) {}

TranslateLanguageList::~TranslateLanguageList() = default;

void TranslateLanguageList::GetSupportedLanguages(
    bool translate_allowed,
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  for (const LanguageTag& tag : supported_languages_) {
    languages->emplace_back(tag.tag_string());
  }

  // Update language lists if they are not updated after Chrome was launched
  // for later requests.
  if (translate_allowed && language_list_fetcher_.get()) {
    RequestLanguageList();
  }
}

// static
std::string TranslateLanguageList::GetLanguageCode(std::string_view language) {
  // Only remove the country code for country specific languages we don't
  // support specifically yet.
  if (IsSupportedLanguage(language)) {
    return std::string(language);
  }
  std::optional<LanguageTag> parsed = GetLanguageTagFromString(language);
  if (parsed) {
    return std::string(parsed->language_subtag());
  }
  return std::string(language);
}

bool TranslateLanguageList::IsSupportedLanguage(std::string_view language) {
  std::optional<LanguageTag> tag = GetLanguageTagFromString(language);
  if (!tag) {
    return false;
  }
  return supported_languages_.contains(*tag);
}

// static
GURL TranslateLanguageList::TranslateLanguageUrl() {
  return GURL(base::StrCat({translate::GetTranslateSecurityOrigin().spec(),
                            kLanguageListFetchPath}));
}

void TranslateLanguageList::RequestLanguageList() {
  request_pending_ = false;
}

void TranslateLanguageList::SetResourceRequestsAllowed(bool allowed) {
  resource_requests_allowed_ = allowed;
  if (resource_requests_allowed_ && request_pending_) {
    RequestLanguageList();
    DCHECK(!request_pending_);
  }
}

base::CallbackListSubscription TranslateLanguageList::RegisterEventCallback(
    const EventCallback& callback) {
  return callback_list_.Add(callback);
}

bool TranslateLanguageList::HasOngoingLanguageListLoadingForTesting() {
  return language_list_fetcher_->state() == TranslateUrlFetcher::REQUESTING;
}

GURL TranslateLanguageList::LanguageFetchURLForTesting() {
  return AddApiKeyToUrl(AddHostLocaleToUrl(TranslateLanguageUrl()));
}

void TranslateLanguageList::OnLanguageListFetchComplete(
    bool success,
    const std::string& data) {
  if (!success) {
    // Since it fails just now, omit to schedule resource requests if
    // ResourceRequestAllowedNotifier think it's ready. Otherwise, a callback
    // will be invoked later to request resources again.
    // The TranslateURLFetcher has a limit for retried requests and aborts
    // re-try not to invoke OnLanguageListFetchComplete anymore if it's asked to
    // re-try too many times.
    NotifyEvent(__LINE__, "Failed to fetch languages");
    return;
  }

  NotifyEvent(__LINE__, "Language list is updated");

  bool parsed_correctly = SetSupportedLanguages(data);
  language_list_fetcher_.reset();

  if (parsed_correctly) {
    last_updated_ = base::Time::Now();
  }
}

void TranslateLanguageList::NotifyEvent(int line, std::string message) {
  TranslateEventDetails details(__FILE__, line, std::move(message));
  callback_list_.Notify(details);
}

bool TranslateLanguageList::SetSupportedLanguages(
    std::string_view language_list) {
  // The format is in JSON as:
  // {
  //   "sl": {"XX": "LanguageName", ...},
  //   "tl": {"XX": "LanguageName", ...}
  // }
  // Where "tl" is set in kTargetLanguagesKey.
  std::optional<base::DictValue> json_value = base::JSONReader::ReadDict(
      language_list, base::JSON_ALLOW_TRAILING_COMMAS);

  if (!json_value) {
    LOG(ERROR) << "Failed to parse language list.";
    // TODO(bug:478219404): Find better way to report this issue.
    return false;
  }
  // The first level dictionary contains two sub-dicts, first for source
  // languages and second for target languages. We want to use the target
  // languages.
  const base::DictValue* target_languages =
      json_value->FindDict(TranslateLanguageList::kTargetLanguagesKey);
  if (!target_languages) {
    LOG(ERROR) << "Target languages not found in translate language list.";
    // TODO(bug:478219404): Find better way to report this issue.
    return false;
  }

  std::vector<LanguageTag> supported_languages_from_service;
  // ... and replace it with the values we just fetched from the server.
  for (auto [lang, language_name] : *target_languages) {
    if (!language::AcceptLanguagesService::CanBeAcceptLanguage(lang.c_str())) {
      // Don't include languages that can not be Accept-Languages
      continue;
    }
    if (std::optional<LanguageTag> language_tag =
            GetLanguageTagFromString(lang);
        language_tag) {
      supported_languages_from_service.emplace_back(*language_tag);
    }
  }

  SortAndUnique(supported_languages_from_service);
  supported_languages_ = base::flat_set<LanguageTag>(
      base::sorted_unique, std::move(supported_languages_from_service));

  std::vector<std::string_view> languages_as_strings;
  std::ranges::transform(supported_languages_,
                         std::back_inserter(languages_as_strings),
                         &LanguageTag::tag_string);

  NotifyEvent(__LINE__, base::JoinString(languages_as_strings, ", "));
  return true;
}

}  // namespace translate
