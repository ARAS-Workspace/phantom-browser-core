// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_prefs.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/i18n/tag_converters.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/language/core/browser/incognito_language_list_map.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/language_detection/core/language_matcher.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace language {
namespace {

base::i18n::LanguageTag ToTranslateLanguageTag(std::string_view language) {
  std::optional<base::i18n::LanguageTag> parsed =
      base::i18n::GetLanguageTagFromString(language);
  if (!parsed) {
    return base::i18n::GetKnownLanguageTag("und");
  }
  return language_detection::GetSupportedLanguageMatcher()
      .Match(*parsed)
      .value_or(*parsed);
}

// Returns the languages that should be blocked by default as a
// base::ListValue.
base::ListValue GetDefaultBlockedLanguages() {
  base::ListValue languages;
  // Accept languages.
#pragma GCC diagnostic push
// See comment above the |break;| in the loop just below for why.
#pragma GCC diagnostic ignored "-Wunreachable-code"
  for (std::string& language :
       base::SplitString(l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES), ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    base::i18n::LanguageTag translate_language_tag =
        ToTranslateLanguageTag(language);
    languages.Append(translate_language_tag.tag_string());

    // crbug.com/958348: The default value for Accept-Language *should* be the
    // same as the one for Blocked Languages. However, Accept-Language contains
    // English (and more) in addition to the local language in most locales due
    // to historical reasons. Exiting early from this loop is a temporary fix
    // that allows Blocked Languages to be at least populated with the UI
    // language while still allowing Translate to trigger on other languages,
    // most importantly English.
    // Once the change to remove English from Accept-Language defaults lands,
    // this break should be removed to enable the Blocked Language List and the
    // Accept-Language list to be initialized to the same values.
    break;
#pragma GCC diagnostic pop
  }

  std::sort(languages.begin(), languages.end());
  languages.erase(std::unique(languages.begin(), languages.end()),
                  languages.end());

  return languages;
}

}  // namespace

void LanguagePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(language::prefs::kAcceptLanguages,
                               l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterStringPref(language::prefs::kSelectedLanguages,
                               std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterListPref(language::prefs::kForcedLanguages);
  registry->RegisterDictionaryPref(
      language::prefs::kPrefNeverPromptSitesWithTime,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      language::prefs::kPrefAlwaysTranslateList,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(language::prefs::kPrefTranslateRecentTarget, "",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(language::prefs::kPrefTranslateRecentTargets,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(language::prefs::kBlockedLanguages,
                             GetDefaultBlockedLanguages(),
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      language::prefs::kAppLanguagePromptShown, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(language::prefs::kULPLanguages);
#endif
}

LanguagePrefs::LanguagePrefs(PrefService* user_prefs) : prefs_(user_prefs) {
  InitializeSelectedLanguagesPref();
  UpdateAcceptLanguagesPref();
  base::RepeatingClosure callback = base::BindRepeating(
      &LanguagePrefs::UpdateAcceptLanguagesPref, base::Unretained(this));
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(language::prefs::kForcedLanguages, callback);
  pref_change_registrar_.Add(language::prefs::kSelectedLanguages, callback);
}

LanguagePrefs::~LanguagePrefs() {
  pref_change_registrar_.RemoveAll();
}

std::vector<std::string> LanguagePrefs::GetNeverTranslateLanguages() const {
  const base::ListValue& fluent_languages_value =
      prefs_->GetList(language::prefs::kBlockedLanguages);

  std::vector<std::string> languages;
  for (const auto& language : fluent_languages_value) {
    const std::string* language_as_string = language.GetIfString();
    // This needs to be checked here as there can be corrupt entries in the pref
    // list which causes a crash.
    if (!language_as_string) {
      continue;
    }

    std::optional<base::i18n::LanguageTag> parsed_tag =
        base::i18n::GetLanguageTagFromString(*language_as_string);
    if (parsed_tag) {
      languages.emplace_back(parsed_tag->tag_string());
    }
  }
  return languages;
}

std::string LanguagePrefs::GetRecentTargetLanguage() const {
  return prefs_->GetString(language::prefs::kPrefTranslateRecentTarget);
}

void LanguagePrefs::GetAcceptLanguagesList(
    std::vector<std::string>* languages) const {
  DCHECK(languages);
  DCHECK(languages->empty());
  const std::string& key = language::prefs::kAcceptLanguages;

  *languages = base::SplitString(prefs_->GetString(key), ",",
                                 base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

void LanguagePrefs::GetUserSelectedLanguagesList(
    std::vector<std::string>* languages) const {
  DCHECK(languages);
  DCHECK(languages->empty());
  const std::string& key = language::prefs::kSelectedLanguages;
  *languages = base::SplitString(prefs_->GetString(key), ",",
                                 base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

void LanguagePrefs::SetUserSelectedLanguagesList(
    const std::vector<std::string>& languages) {
  std::vector<std::string> filtered_languages =
      l10n_util::KeepAcceptedLanguages(languages);
  std::string languages_str = base::JoinString(filtered_languages, ",");
  prefs_->SetString(language::prefs::kSelectedLanguages, languages_str);
}

void LanguagePrefs::GetDeduplicatedUserLanguages(
    std::string* deduplicated_languages_string) {
  std::vector<std::string> deduplicated_languages;
  forced_languages_set_.clear();

  // Add policy languages.
  for (const auto& language :
       prefs_->GetList(language::prefs::kForcedLanguages)) {
    if (forced_languages_set_.find(language.GetString()) ==
        forced_languages_set_.end()) {
      deduplicated_languages.emplace_back(language.GetString());
      forced_languages_set_.insert(language.GetString());
    }
  }

  // Add non-duplicate user-selected languages.
  for (auto& language :
       base::SplitString(prefs_->GetString(language::prefs::kSelectedLanguages),
                         ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (forced_languages_set_.find(language) == forced_languages_set_.end())
      deduplicated_languages.emplace_back(std::move(language));
  }
  *deduplicated_languages_string =
      base::JoinString(deduplicated_languages, ",");
}

void LanguagePrefs::UpdateAcceptLanguagesPref() {
  std::string deduplicated_languages_string;
  GetDeduplicatedUserLanguages(&deduplicated_languages_string);
  if (deduplicated_languages_string !=
      prefs_->GetString(language::prefs::kAcceptLanguages))
    prefs_->SetString(language::prefs::kAcceptLanguages,
                      deduplicated_languages_string);
}

#if BUILDFLAG(IS_ANDROID)
std::vector<std::string> LanguagePrefs::GetULPLanguages() {
  std::vector<std::string> ulp_languages;
  for (const auto& language : prefs_->GetList(language::prefs::kULPLanguages)) {
    ulp_languages.push_back(language.GetString());
  }
  return ulp_languages;
}

void LanguagePrefs::SetULPLanguages(
    std::vector<base::i18n::LanguageTag> ulp_languages) {
  base::ListValue ulp_pref_list;
  ulp_pref_list.reserve(ulp_languages.size());
  for (const auto& language : ulp_languages) {
    ulp_pref_list.Append(std::string(language.tag_string()));
  }
  prefs_->SetList(language::prefs::kULPLanguages, std::move(ulp_pref_list));
}
#endif

bool LanguagePrefs::IsForcedLanguage(std::string_view language) {
  return forced_languages_set_.find(language) != forced_languages_set_.end();
}

void LanguagePrefs::InitializeSelectedLanguagesPref() {
  // Initializes user-selected languages if they're empty.
  // This is important so that previously saved languages aren't overwritten.
  if (prefs_->GetString(language::prefs::kSelectedLanguages).empty()) {
    prefs_->SetString(language::prefs::kSelectedLanguages,
                      prefs_->GetString(language::prefs::kAcceptLanguages));
  }
}

void ResetLanguagePrefs(PrefService* prefs) {
  prefs->ClearPref(language::prefs::kSelectedLanguages);
  prefs->ClearPref(language::prefs::kAcceptLanguages);
#if BUILDFLAG(IS_ANDROID)
  prefs->ClearPref(language::prefs::kULPLanguages);
#endif
}

std::string GetFirstLanguage(std::string_view language_list) {
  auto end = language_list.find(",");
  return std::string(language_list.substr(0, end));
}

namespace {

// Ensure at compile time that our fallback key exists in the generated map.
constexpr bool IncognitoMapContainsFallback() {
  return std::ranges::any_of(kIncognitoLanguageListMap, [](const auto& e) {
    return e.first == kFallbackInputMethodLocale;
  });
}
static_assert(
    IncognitoMapContainsFallback(),
    "kFallbackInputMethodLocale must exist in kIncognitoLanguageListMap");

}  // namespace

std::string GetIncognitoLanguageList(std::string_view language_list) {
  auto comma_pos = language_list.find(',');
  // <2 values suggests a user actively deleted other languages from their
  // settings, so prioritize keeping that user preference over the normal logic.
  if (comma_pos == std::string_view::npos) {
    return std::string(language_list);
  }

  std::string_view first_language = language_list.substr(0, comma_pos);

  // Look up in the generated map.
  auto it = kIncognitoLanguageListMap.find(first_language);
  if (it != kIncognitoLanguageListMap.end()) {
    return std::string(it->second);
  }

  // Fallback for simple (no regional subtags) or unrecognized languages, just
  // prepend the first language to global default (i.e., "en-US,en").
  return base::StrCat(
      {first_language, ",",
       kIncognitoLanguageListMap.at(kFallbackInputMethodLocale)});
}

}  // namespace language
