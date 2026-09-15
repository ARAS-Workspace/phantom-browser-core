// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_prefs.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/language/core/common/language_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/language_detection/core/language_matcher.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/base/l10n/chromium_language_matcher.h"

namespace translate {
namespace {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::GetLanguageTagFromString;
using ::base::i18n::LanguageTag;

constexpr int kForceTriggerBackoffThreshold = 4;

LanguageTag ToTranslateLanguageTag(std::string_view language) {
  std::optional<LanguageTag> parsed = GetLanguageTagFromString(language);
  if (!parsed) {
    return GetKnownLanguageTag("und");
  }
  return language_detection::GetSupportedLanguageMatcher()
      .Match(*parsed)
      .value_or(*parsed);
}

LanguageTag ResolveLanguagTag(const LanguageTag& language_tag) {
  return language_detection::GetSupportedLanguageMatcher()
      .Match(language_tag)
      .value_or(language_tag);
}

// Merge old always-translate languages from the deprecated pref to the new
// version. Because of crbug/1291356, it's possible that on iOS the client
// started using the new pref without properly migrating and clearing the old
// pref. This function will avoid merging values from the old pref that seem to
// conflict with values already present in the new pref.
void MigrateObsoleteAlwaysTranslateLanguagesPref(PrefService* prefs) {
  const base::DictValue& deprecated_dictionary =
      prefs->GetDict(TranslatePrefs::kPrefAlwaysTranslateListDeprecated);
  // Migration is performed only once per client, since the deprecated pref is
  // cleared after migration. This will make subsequent calls to migrate no-ops.
  if (deprecated_dictionary.empty())
    return;

  ScopedDictPrefUpdate always_translate_dictionary_update(
      prefs, language::prefs::kPrefAlwaysTranslateList);
  base::DictValue& always_translate_dictionary =
      always_translate_dictionary_update.Get();

  for (const auto old_language_pair : deprecated_dictionary) {
    // If the old pref's language pair conflicts with any of the new pref's
    // language pairs, where either the new pref already specifies behavior
    // about always translating from or to the old source language, or always
    // translating from the old target language, then skip merging this pair
    // into the new pref.
    if (std::ranges::any_of(
            always_translate_dictionary,
            [&old_language_pair](const auto& new_language_pair) {
              return old_language_pair.first == new_language_pair.first ||
                     old_language_pair.first ==
                         new_language_pair.second.GetString() ||
                     old_language_pair.second.GetString() ==
                         new_language_pair.first;
            })) {
      continue;
    }

    // If the old pair's source language matches any of the never-translate
    // languages, it probably means that this source language was set to never
    // be translated after the old pref was deprecated, so avoid this conflict.
    if (prefs->GetList(language::prefs::kBlockedLanguages)
            .contains(old_language_pair.first)) {
      continue;
    }

    always_translate_dictionary.Set(old_language_pair.first,
                                    old_language_pair.second.GetString());
  }

  prefs->ClearPref(TranslatePrefs::kPrefAlwaysTranslateListDeprecated);
}

bool IsTranslateLanguage(std::string_view language) {
  // Check if |language| is translatable.
  TranslateLanguageList* language_list =
      TranslateDownloadManager::GetInstance()->language_list();
  return language_list && language_list->IsSupportedLanguage(language);
}
}  // namespace

// The below properties used to be used but now are deprecated. Don't use them
// since an old profile might have some values there.
//
// * translate_last_denied_time
// * translate_too_often_denied
// * translate_language_blacklist

BASE_FEATURE(kTranslateRecentTarget, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMigrateAlwaysTranslateLanguagesFix,
             base::FEATURE_ENABLED_BY_DEFAULT);

TranslateLanguageInfo::TranslateLanguageInfo() = default;

TranslateLanguageInfo::TranslateLanguageInfo(const TranslateLanguageInfo&) =
    default;
TranslateLanguageInfo::TranslateLanguageInfo(TranslateLanguageInfo&&) noexcept =
    default;
TranslateLanguageInfo& TranslateLanguageInfo::operator=(
    const TranslateLanguageInfo&) = default;
TranslateLanguageInfo& TranslateLanguageInfo::operator=(
    TranslateLanguageInfo&&) noexcept = default;

TranslatePrefs::TranslatePrefs(PrefService* user_prefs)
    : prefs_(user_prefs),
      language_prefs_(std::make_unique<language::LanguagePrefs>(user_prefs)) {
  MigrateNeverPromptSites();
  ResetEmptyBlockedLanguagesToDefaults();

  if (base::FeatureList::IsEnabled(kMigrateAlwaysTranslateLanguagesFix))
    MigrateObsoleteAlwaysTranslateLanguagesPref(user_prefs);
}

TranslatePrefs::~TranslatePrefs() = default;

// static
std::string TranslatePrefs::MapPreferenceName(std::string_view pref_name) {
  if (pref_name == kPrefNeverPromptSitesDeprecated) {
    return "translate_site_blocklist";
  }
  return std::string(pref_name);
}

bool TranslatePrefs::IsOfferTranslateEnabled() const {
  return false;
}

bool TranslatePrefs::IsTranslateAllowedByPolicy() const {
  return false;
}

void TranslatePrefs::SetCountry(std::string_view country) {
  country_ = std::string(country);
}

std::string TranslatePrefs::GetCountry() const {
  return country_;
}

void TranslatePrefs::ResetToDefaults() {
  ResetBlockedLanguagesToDefault();
  ClearNeverPromptSiteList();
  ClearAlwaysTranslateLanguagePairs();
  prefs_->ClearPref(kPrefTranslateDeniedCount);
  prefs_->ClearPref(kPrefTranslateIgnoredCount);
  prefs_->ClearPref(kPrefTranslateAcceptedCount);
  prefs_->ClearPref(language::prefs::kPrefTranslateRecentTarget);
  force_translate_on_english_for_testing_ = false;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  prefs_->ClearPref(kPrefTranslateAutoAlwaysCount);
  prefs_->ClearPref(kPrefTranslateAutoNeverCount);
#endif

  prefs_->ClearPref(language::prefs::kOfferTranslateEnabled);
}

bool TranslatePrefs::IsBlockedLanguage(std::string_view input_language) const {
  return language_prefs_->IsBlockedLanguage(input_language);
}

void TranslatePrefs::BlockLanguage(std::string_view input_language) {
  language_prefs_->BlockLanguage(input_language);
  // Remove the blocked language from the always translate list if present.
  RemoveLanguagePairFromAlwaysTranslateList(input_language);
}

void TranslatePrefs::UnblockLanguage(std::string_view input_language) {
  language_prefs_->UnblockLanguage(input_language);
}

void TranslatePrefs::ResetEmptyBlockedLanguagesToDefaults() {
  if (GetNeverTranslateLanguages().size() == 0) {
    ResetBlockedLanguagesToDefault();
  }
}

void TranslatePrefs::ResetBlockedLanguagesToDefault() {
  prefs_->ClearPref(language::prefs::kBlockedLanguages);
}

std::vector<std::string> TranslatePrefs::GetNeverTranslateLanguages() const {
  return language_prefs_->GetNeverTranslateLanguages();
}

// Note: the language codes used in the language settings list have the Chrome
// internal format and not the Translate server format.
// To convert from one to the other use util functions
// ToTranslateLanguageTag() and base::i18n::LanguageTagConverter.
void TranslatePrefs::AddToLanguageList(
    const base::i18n::LanguageTag& language_tag,
    const bool force_blocked) {
  language_prefs_->AddToLanguageList(language_tag, force_blocked);
}

void TranslatePrefs::RemoveFromLanguageList(
    const base::i18n::LanguageTag& language_tag) {
  language_prefs_->RemoveFromLanguageList(language_tag);
}

void TranslatePrefs::RearrangeLanguage(
    std::string_view language,
    const TranslatePrefs::RearrangeSpecifier where,
    int offset,
    const std::vector<std::string>& enabled_languages) {
  language::LanguagePrefs::RearrangeSpecifier target =
      language::LanguagePrefs::kNone;
  switch (where) {
    case kNone:
      target = language::LanguagePrefs::kNone;
      break;
    case kTop:
      target = language::LanguagePrefs::kTop;
      break;
    case kUp:
      target = language::LanguagePrefs::kUp;
      break;
    case kDown:
      target = language::LanguagePrefs::kDown;
      break;
  }
  language_prefs_->RearrangeLanguage(language, target, offset,
                                     enabled_languages);
}

void TranslatePrefs::SetLanguageOrder(
    const std::vector<std::string>& new_order) {
  language_prefs_->SetUserSelectedLanguagesList(new_order);
}

// static
void TranslatePrefs::GetLanguageInfoList(
    const std::string& app_locale,
    bool translate_allowed,
    std::vector<TranslateLanguageInfo>* language_list) {
  DCHECK(language_list != nullptr);

  if (app_locale.empty()) {
    return;
  }

  std::vector<language::LanguageInfo> languages;
  language::LanguagePrefs::GetLanguageInfoList(app_locale, &languages);

  if (translate_allowed) {
    TranslateDownloadManager::RequestLanguageList();
  }

  language_list->clear();
  for (auto& entry : languages) {
    TranslateLanguageInfo language;
    language.code = std::move(entry.code);
    language.display_name = std::move(entry.display_name);
    language.native_display_name = std::move(entry.native_display_name);

    // Extract the base language: if the base language can be translated,
    // then even the regional one should be marked as such.
    LanguageTag translate_code = ToTranslateLanguageTag(language.code);
    language.supports_translate = TranslateDownloadManager::IsSupportedLanguage(
        translate_code.tag_string());
    language_list->push_back(std::move(language));
  }
}

void TranslatePrefs::GetTranslatableContentLanguages(
    const std::string& app_locale,
    std::vector<std::string>* codes) {
  DCHECK(codes != nullptr);

  if (app_locale.empty()) {
    return;
  }
  codes->clear();

  // Get the language codes of user content languages.
  // Returned in Chrome format.
  std::vector<base::i18n::LanguageTag> language_tags = GetLanguageList();

  absl::flat_hash_set<std::string> unique_languages;
  unique_languages.reserve(language_tags.size());
  for (const auto& entry : language_tags) {
    // Get the language in Translate format.
    LanguageTag supports_translate_code = ResolveLanguagTag(entry);
    // Extract the language code, for example for en-US it returns en.
    std::string lang_code = TranslateDownloadManager::GetLanguageCode(
        supports_translate_code.tag_string());
    // If the language code for a translatable language hasn't yet been added,
    // add it to the result list.
    if (TranslateDownloadManager::IsSupportedLanguage(lang_code)) {
      if (unique_languages.insert(lang_code).second) {
        codes->push_back(std::move(lang_code));
      }
    }
  }
}

bool TranslatePrefs::IsSiteOnNeverPromptList(std::string_view site) const {
  return prefs_->GetDict(language::prefs::kPrefNeverPromptSitesWithTime)
      .Find(site);
}

void TranslatePrefs::AddSiteToNeverPromptList(std::string_view site,
                                              base::Time time) {
  DCHECK(!site.empty());
  AddValueToNeverPromptList(kPrefNeverPromptSitesDeprecated, site);
  ScopedDictPrefUpdate update(prefs_,
                              language::prefs::kPrefNeverPromptSitesWithTime);
  update->Set(site, base::TimeToValue(time));
}

void TranslatePrefs::AddSiteToNeverPromptList(std::string_view site) {
  AddSiteToNeverPromptList(site, base::Time::Now());
}

void TranslatePrefs::RemoveSiteFromNeverPromptList(std::string_view site) {
  DCHECK(!site.empty());
  RemoveValueFromNeverPromptList(kPrefNeverPromptSitesDeprecated, site);
  ScopedDictPrefUpdate update(prefs_,
                              language::prefs::kPrefNeverPromptSitesWithTime);
  update->Remove(site);
}

std::vector<std::string> TranslatePrefs::GetNeverPromptSitesBetween(
    base::Time begin,
    base::Time end) const {
  std::vector<std::string> result;
  const auto& dict =
      prefs_->GetDict(language::prefs::kPrefNeverPromptSitesWithTime);
  for (const auto entry : dict) {
    std::optional<base::Time> time = base::ValueToTime(entry.second);
    if (!time) {
      // Badly formatted preferences may be synced from the server, see
      // https://crbug.com/1295549
      LOG(ERROR) << "Preference "
                 << language::prefs::kPrefNeverPromptSitesWithTime
                 << " has invalid format. Ignoring.";
      continue;
    }
    if (begin <= *time && *time < end)
      result.push_back(entry.first);
  }
  return result;
}

void TranslatePrefs::DeleteNeverPromptSitesBetween(base::Time begin,
                                                   base::Time end) {
  for (auto& site : GetNeverPromptSitesBetween(begin, end))
    RemoveSiteFromNeverPromptList(site);
}

bool TranslatePrefs::IsLanguagePairOnAlwaysTranslateList(
    std::string_view source_language,
    std::string_view target_language) {
  const base::DictValue& dict =
      prefs_->GetDict(language::prefs::kPrefAlwaysTranslateList);

  const std::string* auto_target_lang = dict.FindString(source_language);
  if (auto_target_lang && *auto_target_lang == target_language)
    return true;

  return false;
}

void TranslatePrefs::AddLanguagePairToAlwaysTranslateList(
    std::string_view source_language,
    std::string_view target_language) {
  // Get translate version of language codes.
  LanguageTag translate_source_language =
      ToTranslateLanguageTag(source_language);
  LanguageTag translate_target_language =
      ToTranslateLanguageTag(target_language);
  if (!IsTranslateLanguage(translate_source_language.tag_string()) ||
      !IsTranslateLanguage(translate_target_language.tag_string())) {
    return;
  }

  ScopedDictPrefUpdate update(prefs_,
                              language::prefs::kPrefAlwaysTranslateList);

  update->Set(translate_source_language.tag_string(),
              translate_target_language.tag_string());
  // Remove source language from block list if present.
  UnblockLanguage(translate_source_language.tag_string());
}

void TranslatePrefs::RemoveLanguagePairFromAlwaysTranslateList(
    std::string_view source_language) {
  ScopedDictPrefUpdate update(prefs_,
                              language::prefs::kPrefAlwaysTranslateList);

  // Get translate version of language codes.
  LanguageTag translate_source_language =
      ToTranslateLanguageTag(source_language);
  update->Remove(translate_source_language.tag_string());
}

std::vector<std::string> TranslatePrefs::GetAlwaysTranslateLanguages() const {
  const base::DictValue& dict =
      prefs_->GetDict(language::prefs::kPrefAlwaysTranslateList);

  std::vector<std::string> languages;
  languages.reserve(dict.size());
  for (auto language_pair : dict) {
    std::optional<LanguageTag> parsed_tag =
        GetLanguageTagFromString(language_pair.first);
    if (!parsed_tag) {
      continue;
    }
    languages.emplace_back(parsed_tag->tag_string());
  }
  return languages;
}

void TranslatePrefs::ClearNeverPromptSiteList() {
  prefs_->ClearPref(kPrefNeverPromptSitesDeprecated);
  prefs_->ClearPref(language::prefs::kPrefNeverPromptSitesWithTime);
}

bool TranslatePrefs::HasLanguagePairsToAlwaysTranslate() const {
  return !IsDictionaryEmpty(language::prefs::kPrefAlwaysTranslateList);
}

void TranslatePrefs::ClearAlwaysTranslateLanguagePairs() {
  prefs_->ClearPref(language::prefs::kPrefAlwaysTranslateList);
}

int TranslatePrefs::GetTranslationDeniedCount(std::string_view language) const {
  const base::DictValue& dict = prefs_->GetDict(kPrefTranslateDeniedCount);
  return dict.FindInt(language).value_or(0);
}

void TranslatePrefs::IncrementTranslationDeniedCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateDeniedCount);
  base::DictValue& dict = update.Get();

  int count = dict.FindInt(language).value_or(0);
  if (count < std::numeric_limits<int>::max())
    dict.Set(language, count + 1);
}

void TranslatePrefs::ResetTranslationDeniedCount(std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateDeniedCount);
  update->Set(language, 0);
}

int TranslatePrefs::GetTranslationIgnoredCount(
    std::string_view language) const {
  const base::DictValue& dict = prefs_->GetDict(kPrefTranslateIgnoredCount);
  return dict.FindInt(language).value_or(0);
}

void TranslatePrefs::IncrementTranslationIgnoredCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateIgnoredCount);
  base::DictValue& dict = update.Get();

  int count = dict.FindInt(language).value_or(0);
  if (count < std::numeric_limits<int>::max())
    dict.Set(language, count + 1);
}

void TranslatePrefs::ResetTranslationIgnoredCount(std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateIgnoredCount);
  update->Set(language, 0);
}

int TranslatePrefs::GetTranslationAcceptedCount(
    std::string_view language) const {
  const base::DictValue& dict = prefs_->GetDict(kPrefTranslateAcceptedCount);
  return dict.FindInt(language).value_or(0);
}

void TranslatePrefs::IncrementTranslationAcceptedCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAcceptedCount);
  base::DictValue& dict = update.Get();

  int count = dict.FindInt(language).value_or(0);
  if (count < std::numeric_limits<int>::max())
    dict.Set(language, count + 1);
}

void TranslatePrefs::ResetTranslationAcceptedCount(std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAcceptedCount);
  update->Set(language, 0);
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
int TranslatePrefs::GetTranslationAutoAlwaysCount(
    std::string_view language) const {
  const base::DictValue& dict = prefs_->GetDict(kPrefTranslateAutoAlwaysCount);
  return dict.FindInt(language).value_or(0);
}

void TranslatePrefs::IncrementTranslationAutoAlwaysCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAutoAlwaysCount);
  base::DictValue& dict = update.Get();

  int count = dict.FindInt(language).value_or(0);
  if (count < std::numeric_limits<int>::max())
    dict.Set(language, count + 1);
}

void TranslatePrefs::ResetTranslationAutoAlwaysCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAutoAlwaysCount);
  update->Set(language, 0);
}

int TranslatePrefs::GetTranslationAutoNeverCount(
    std::string_view language) const {
  const base::DictValue& dict = prefs_->GetDict(kPrefTranslateAutoNeverCount);
  return dict.FindInt(language).value_or(0);
}

void TranslatePrefs::IncrementTranslationAutoNeverCount(
    std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAutoNeverCount);
  base::DictValue& dict = update.Get();

  int count = dict.FindInt(language).value_or(0);
  if (count < std::numeric_limits<int>::max())
    dict.Set(language, count + 1);
}

void TranslatePrefs::ResetTranslationAutoNeverCount(std::string_view language) {
  ScopedDictPrefUpdate update(prefs_, kPrefTranslateAutoNeverCount);
  update->Set(language, 0);
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
bool TranslatePrefs::GetAppLanguagePromptShown() const {
  return prefs_->GetBoolean(language::prefs::kAppLanguagePromptShown);
}

void TranslatePrefs::SetAppLanguagePromptShown() {
  prefs_->SetBoolean(language::prefs::kAppLanguagePromptShown, true);
}
#endif  // BUILDFLAG(IS_ANDROID)

std::vector<base::i18n::LanguageTag> TranslatePrefs::GetLanguageList() const {
  std::vector<base::i18n::LanguageTag> languages;
  std::vector<std::string> language_codes;
  language_prefs_->GetAcceptLanguagesList(&language_codes);
  languages.reserve(language_codes.size());
  for (const auto& code : language_codes) {
    if (std::optional<base::i18n::LanguageTag> parsed_tag =
            GetLanguageTagFromString(code)) {
      languages.push_back(*parsed_tag);
    }
  }
  return languages;
}

std::vector<base::i18n::LanguageTag>
TranslatePrefs::GetUserSelectedLanguageList() const {
  std::vector<base::i18n::LanguageTag> languages;
  std::vector<std::string> language_codes;
  language_prefs_->GetUserSelectedLanguagesList(&language_codes);
  languages.reserve(language_codes.size());
  for (const auto& code : language_codes) {
    if (std::optional<base::i18n::LanguageTag> parsed_tag =
            GetLanguageTagFromString(code)) {
      languages.push_back(*parsed_tag);
    }
  }
  return languages;
}

bool TranslatePrefs::ShouldForceTriggerTranslateOnEnglishPages() {
  if (!language::OverrideTranslateTriggerInIndia() &&
      !force_translate_on_english_for_testing_) {
    return false;
  }

  return GetForceTriggerOnEnglishPagesCount() < kForceTriggerBackoffThreshold;
}

bool TranslatePrefs::force_translate_on_english_for_testing_ = false;

// static
void TranslatePrefs::SetShouldForceTriggerTranslateOnEnglishPagesForTesting() {
  force_translate_on_english_for_testing_ = true;
}

bool TranslatePrefs::CanTranslateLanguage(std::string_view language) {
  // Under this experiment, translate English page even though English may be
  // blocked.
  if (language == "en" && ShouldForceTriggerTranslateOnEnglishPages()) {
    return true;
  }

  return !IsBlockedLanguage(language);
}

bool TranslatePrefs::ShouldAutoTranslate(std::string_view source_language,
                                         std::string* target_language) {
  const base::DictValue& dict =
      prefs_->GetDict(language::prefs::kPrefAlwaysTranslateList);

  const std::string* value = dict.FindString(source_language);
  if (!value)
    return false;

  DCHECK(!value->empty());
  target_language->assign(*value);
  return true;
}

void TranslatePrefs::SetRecentTargetLanguage(std::string_view target_language) {
  if (target_language.empty()) {
    prefs_->SetString(language::prefs::kPrefTranslateRecentTarget, "");
    return;
  }
  // Get translate version of language code.
  LanguageTag translate_target_language =
      ToTranslateLanguageTag(target_language);
  prefs_->SetString(language::prefs::kPrefTranslateRecentTarget,
                    translate_target_language.tag_string());
  // Update the recent target languages list.
  ScopedListPrefUpdate update(prefs_,
                              language::prefs::kPrefTranslateRecentTargets);
  base::ListValue& recent_targets = update.Get();
  recent_targets.EraseValue(
      base::Value(translate_target_language.tag_string()));
  recent_targets.Insert(recent_targets.begin(),
                        base::Value(translate_target_language.tag_string()));
  // Limit the list to the last 3 target languages.
  if (recent_targets.size() > 3) {
    recent_targets.erase(recent_targets.begin() + 3, recent_targets.end());
  }
}

void TranslatePrefs::ResetRecentTargetLanguage() {
  SetRecentTargetLanguage("");
  prefs_->ClearPref(language::prefs::kPrefTranslateRecentTargets);
}

std::string TranslatePrefs::GetRecentTargetLanguage() const {
  return language_prefs_->GetRecentTargetLanguage();
}

std::vector<std::string> TranslatePrefs::GetRecentTargetLanguages() const {
  std::vector<std::string> result;
  for (const auto& value :
       prefs_->GetList(language::prefs::kPrefTranslateRecentTargets)) {
    if (value.is_string()) {
      result.push_back(value.GetString());
    }
  }
  // For backward compatibility, if the list is empty but the legacy string pref
  // is present, we can add it.
  if (result.empty()) {
    std::string legacy_recent = GetRecentTargetLanguage();
    if (!legacy_recent.empty()) {
      result.push_back(legacy_recent);
    }
  }
  return result;
}

int TranslatePrefs::GetForceTriggerOnEnglishPagesCount() const {
  return prefs_->GetInteger(kPrefForceTriggerTranslateCount);
}

void TranslatePrefs::ReportForceTriggerOnEnglishPages() {
  int current_count = GetForceTriggerOnEnglishPagesCount();
  if (current_count != -1 && current_count < std::numeric_limits<int>::max())
    prefs_->SetInteger(kPrefForceTriggerTranslateCount, current_count + 1);
}

void TranslatePrefs::ReportAcceptedAfterForceTriggerOnEnglishPages() {
  int current_count = GetForceTriggerOnEnglishPagesCount();
  if (current_count != -1)
    prefs_->SetInteger(kPrefForceTriggerTranslateCount, -1);
}

// static
void TranslatePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kPrefNeverPromptSitesDeprecated);
  registry->RegisterDictionaryPref(
      kPrefTranslateDeniedCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(kPrefTranslateIgnoredCount);
  registry->RegisterDictionaryPref(
      kPrefTranslateAcceptedCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kPrefForceTriggerTranslateCount, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  registry->RegisterDictionaryPref(
      kPrefTranslateAutoAlwaysCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateAutoNeverCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif

  RegisterProfilePrefsForMigration(registry);
}

// static
void TranslatePrefs::RegisterProfilePrefsForMigration(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(crbug.com/40826252): Deprecated 10/2021. Check status of bug before
  // removing.
  registry->RegisterDictionaryPref(kPrefAlwaysTranslateListDeprecated);
}

void TranslatePrefs::MigrateNeverPromptSites() {
  // Migration copies any sites on the deprecated never prompt pref to
  // the new version and clears all references to the old one. This will
  // make subsequent calls to migrate no-ops.

  // Early-out when there's nothing to migrate. This avoids constructing a
  // ScopedListPrefUpdate (which unconditionally dirties the pref store on
  // destruction) on every navigation after migration is already complete.
  const base::ListValue& deprecated_list =
      prefs_->GetList(kPrefNeverPromptSitesDeprecated);
  if (deprecated_list.empty()) {
    return;
  }

  base::DictValue never_prompt_list =
      prefs_->GetDict(language::prefs::kPrefNeverPromptSitesWithTime).Clone();
  bool migrated_any = false;
  for (const auto& site : deprecated_list) {
    if (site.is_string() &&
        (!never_prompt_list.Find(site.GetString()) ||
         !base::ValueToTime(never_prompt_list.Find(site.GetString())))) {
      never_prompt_list.Set(site.GetString(),
                            base::TimeToValue(base::Time::Now()));
      migrated_any = true;
    }
  }
  if (migrated_any) {
    prefs_->SetDict(language::prefs::kPrefNeverPromptSitesWithTime,
                    std::move(never_prompt_list));
  }
  prefs_->ClearPref(kPrefNeverPromptSitesDeprecated);
}

bool TranslatePrefs::IsValueOnNeverPromptList(const char* pref_id,
                                              std::string_view value) const {
  const base::ListValue& never_prompt_list = prefs_->GetList(pref_id);
  for (const base::Value& value_in_list : never_prompt_list) {
    if (value_in_list.is_string() && value_in_list.GetString() == value)
      return true;
  }
  return false;
}

void TranslatePrefs::AddValueToNeverPromptList(const char* pref_id,
                                               std::string_view value) {
  ScopedListPrefUpdate update(prefs_, pref_id);
  base::ListValue& never_prompt_list = update.Get();

  if (IsValueOnNeverPromptList(pref_id, value)) {
    return;
  }
  never_prompt_list.Append(value);
}

void TranslatePrefs::RemoveValueFromNeverPromptList(const char* pref_id,
                                                    std::string_view value) {
  ScopedListPrefUpdate update(prefs_, pref_id);
  base::ListValue& never_prompt_list = update.Get();

  auto value_to_erase = std::ranges::find_if(
      never_prompt_list, [value](const base::Value& value_in_list) {
        return value_in_list.is_string() && value_in_list.GetString() == value;
      });
  if (value_to_erase != never_prompt_list.end())
    never_prompt_list.erase(value_to_erase);
}

size_t TranslatePrefs::GetListSize(const char* pref_id) const {
  const base::ListValue& never_prompt_list = prefs_->GetList(pref_id);
  return never_prompt_list.size();
}

bool TranslatePrefs::IsDictionaryEmpty(const char* pref_id) const {
  const base::DictValue& dict = prefs_->GetDict(pref_id);
  return (dict.empty());
}
}  // namespace translate
