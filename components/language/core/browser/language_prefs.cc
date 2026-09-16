// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_prefs.h"

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/i18n/tag_converters.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

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

// Returns whether or not the given list includes at least one language with
// the same base as the input language.
// For example: "en-US" and "en-UK" share the same base "en".
bool ContainsSameBaseLanguage(const std::vector<base::i18n::LanguageTag>& list,
                              std::string_view language_code) {
  std::optional<base::i18n::LanguageTag> parsed_input =
      base::i18n::GetLanguageTagFromString(language_code);
  if (!parsed_input) {
    return false;
  }
  for (const auto& item : list) {
    if (parsed_input->language_subtag() == item.language_subtag()) {
      return true;
    }
  }
  return false;
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

LanguageInfo::LanguageInfo() = default;
LanguageInfo::LanguageInfo(const LanguageInfo&) = default;
LanguageInfo::LanguageInfo(LanguageInfo&&) noexcept = default;
LanguageInfo& LanguageInfo::operator=(const LanguageInfo&) = default;
LanguageInfo& LanguageInfo::operator=(LanguageInfo&&) noexcept = default;

bool LanguagePrefs::IsBlockedLanguage(std::string_view input_language) const {
  base::i18n::LanguageTag canonical_lang =
      ToTranslateLanguageTag(input_language);
  const base::ListValue& blocked =
      prefs_->GetList(language::prefs::kBlockedLanguages);
  return blocked.contains(canonical_lang.tag_string());
}

void LanguagePrefs::BlockLanguage(std::string_view input_language) {
  DCHECK(!input_language.empty());

  // Get the translate version of the string to add to the blocked list.
  base::i18n::LanguageTag translate_lang =
      ToTranslateLanguageTag(input_language);

  // If neither the translate or Chrome language is a possible accept
  // language skip adding to blocked language list.
  if (!l10n_util::IsPossibleAcceptLanguage(translate_lang.tag_string())) {
    return;
  }

  if (!IsBlockedLanguage(translate_lang.tag_string())) {
    ScopedListPrefUpdate update(prefs_, language::prefs::kBlockedLanguages);
    update->Append(translate_lang.tag_string());
  }
}

void LanguagePrefs::UnblockLanguage(std::string_view input_language) {
  DCHECK(!input_language.empty());
  // Never remove last fluent language.
  if (GetNeverTranslateLanguages().size() <= 1) {
    return;
  }
  base::i18n::LanguageTag canonical_lang =
      ToTranslateLanguageTag(input_language);
  ScopedListPrefUpdate update(prefs_, language::prefs::kBlockedLanguages);
  update->EraseValue(base::Value(canonical_lang.tag_string()));
}

std::vector<base::i18n::LanguageTag> LanguagePrefs::GetLanguageList() const {
  std::vector<base::i18n::LanguageTag> languages;
  std::vector<std::string> language_codes;
  GetAcceptLanguagesList(&language_codes);
  languages.reserve(language_codes.size());
  for (const auto& code : language_codes) {
    if (std::optional<base::i18n::LanguageTag> parsed_tag =
            base::i18n::GetLanguageTagFromString(code)) {
      languages.push_back(*parsed_tag);
    }
  }
  return languages;
}

std::vector<base::i18n::LanguageTag>
LanguagePrefs::GetUserSelectedLanguageList() const {
  std::vector<base::i18n::LanguageTag> languages;
  std::vector<std::string> language_codes;
  GetUserSelectedLanguagesList(&language_codes);
  languages.reserve(language_codes.size());
  for (const auto& code : language_codes) {
    if (std::optional<base::i18n::LanguageTag> parsed_tag =
            base::i18n::GetLanguageTagFromString(code)) {
      languages.push_back(*parsed_tag);
    }
  }
  return languages;
}

void LanguagePrefs::AddToLanguageList(
    const base::i18n::LanguageTag& language_tag,
    const bool force_blocked) {
  DCHECK(!language_tag.tag_string().empty());

  std::vector<base::i18n::LanguageTag> languages = GetLanguageList();
  std::vector<base::i18n::LanguageTag> user_selected_languages =
      GetUserSelectedLanguageList();

  // We should block the language if the list does not already contain another
  // language with the same base language. Policy-forced languages aren't
  // counted as "blocking", so only user-selected languages are checked.
  const bool should_block = !ContainsSameBaseLanguage(
      user_selected_languages, language_tag.tag_string());

  if (force_blocked || should_block) {
    BlockLanguage(language_tag.tag_string());
  }

  // Add the language to the list.
  if (!std::ranges::contains(languages, language_tag)) {
    user_selected_languages.push_back(language_tag);
    std::vector<std::string> user_selected_language_codes;
    user_selected_language_codes.reserve(user_selected_languages.size());
    for (const auto& tag : user_selected_languages) {
      user_selected_language_codes.push_back(std::string(tag.tag_string()));
    }
    SetUserSelectedLanguagesList(user_selected_language_codes);
  }
}

void LanguagePrefs::RemoveFromLanguageList(
    const base::i18n::LanguageTag& language_tag) {
  DCHECK(!language_tag.tag_string().empty());

  std::vector<base::i18n::LanguageTag> user_selected_languages =
      GetUserSelectedLanguageList();

  // Remove the language from the list.
  const auto& it = std::ranges::find(user_selected_languages, language_tag);
  if (it != user_selected_languages.end()) {
    user_selected_languages.erase(it);
    std::vector<std::string> user_selected_language_codes;
    user_selected_language_codes.reserve(user_selected_languages.size());
    for (const auto& tag : user_selected_languages) {
      user_selected_language_codes.push_back(std::string(tag.tag_string()));
    }
    SetUserSelectedLanguagesList(user_selected_language_codes);

    // We should unblock the language if this was the last one from the same
    // language family.
    if (!ContainsSameBaseLanguage(GetLanguageList(),
                                  language_tag.tag_string())) {
      UnblockLanguage(language_tag.tag_string());
    }
  }
}

void LanguagePrefs::RearrangeLanguage(
    std::string_view language,
    const LanguagePrefs::RearrangeSpecifier where,
    int offset,
    const std::vector<std::string>& enabled_languages) {
  // Negative offset is not supported.
  DCHECK(!(offset < 1 && (where == kUp || where == kDown)));

  std::vector<std::string> languages;
  for (const auto& tag : GetUserSelectedLanguageList()) {
    languages.push_back(std::string(tag.tag_string()));
  }

  auto pos = std::ranges::find(languages, language);
  if (pos == languages.end()) {
    return;
  }

  // Sort the vector of enabled languages for fast lookup.
  std::vector<std::string_view> enabled(enabled_languages.begin(),
                                        enabled_languages.end());
  std::sort(enabled.begin(), enabled.end());
  if (!std::binary_search(enabled.begin(), enabled.end(), language)) {
    return;
  }

  switch (where) {
    case kTop:
      // To avoid code duplication, set |offset| to max int and re-use the logic
      // to move |language| up in the list as far as possible.
      offset = std::numeric_limits<int>::max();
      [[fallthrough]];
    case kUp:
      if (pos == languages.begin()) {
        return;
      }
      while (pos != languages.begin()) {
        auto next_pos = pos - 1;
        // Skip over non-enabled languages without decrementing |offset|.
        // Also skip over languages hidden due to duplication between forced
        // and user-selected languages.
        if (std::binary_search(enabled.begin(), enabled.end(), *next_pos) &&
            !IsForcedLanguage(*next_pos)) {
          // By only checking |offset| when an enabled, non-forced language is
          // found, and decrementing |offset| after checking it (instead of
          // before), this means that |language| will be moved up the list until
          // it has either reached the next enabled language or the top of the
          // list.
          if (offset <= 0) {
            break;
          }
          --offset;
        }
        std::swap(*next_pos, *pos);
        pos = next_pos;
      }
      break;

    case kDown:
      if (pos + 1 == languages.end()) {
        return;
      }
      for (auto next_pos = pos + 1; next_pos != languages.end() && offset > 0;
           pos = next_pos++) {
        // Skip over non-enabled or forced languages without decrementing
        // offset. Unlike moving languages up in the list, moving languages down
        // in the list stops as soon as |offset| reaches zero, instead of
        // continuing to skip non-enabled languages after |offset| has reached
        // zero.
        if (std::binary_search(enabled.begin(), enabled.end(), *next_pos) &&
            !IsForcedLanguage(*next_pos)) {
          --offset;
        }
        std::swap(*next_pos, *pos);
      }
      break;

    case kNone:
      return;

    default:
      NOTREACHED();
  }

  SetUserSelectedLanguagesList(languages);
}

// static
void LanguagePrefs::GetLanguageInfoList(
    const std::string& app_locale,
    std::vector<LanguageInfo>* language_list) {
  DCHECK(language_list != nullptr);

  if (app_locale.empty()) {
    return;
  }

  language_list->clear();

  // Collect the language codes from the supported accept-languages.
  std::vector<std::string> language_codes =
      l10n_util::GetAcceptLanguagesForLocale(app_locale);

  // Collator used to sort display names in the given locale.
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(icu::Locale(app_locale.c_str()), error));
  if (U_FAILURE(error)) {
    collator.reset();
  }
  // Map of [display name -> language code].
  std::map<std::u16string, std::string,
           l10n_util::StringComparator<std::u16string>>
      language_map(l10n_util::StringComparator<std::u16string>(collator.get()));

  // Build the list of display names and the language map.
  for (std::string& code : language_codes) {
    language_map[l10n_util::GetDisplayNameForLocale(code, app_locale, false)] =
        std::move(code);
  }

  // Build the language list from the language map.
  for (auto& entry : language_map) {
    LanguageInfo language;
    language.code = std::move(entry.second);

    std::u16string adjusted_display_name = entry.first;
    base::i18n::AdjustStringForLocaleDirection(&adjusted_display_name);
    language.display_name = base::UTF16ToUTF8(adjusted_display_name);

    std::u16string adjusted_native_display_name =
        l10n_util::GetDisplayNameForLocale(language.code, language.code, false);
    base::i18n::AdjustStringForLocaleDirection(&adjusted_native_display_name);
    language.native_display_name =
        base::UTF16ToUTF8(adjusted_native_display_name);
    language_list->push_back(std::move(language));
  }
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
  prefs->ClearPref(language::prefs::kBlockedLanguages);
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
