// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace base {
class Value;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

namespace language {

inline constexpr char kFallbackInputMethodLocale[] = "en-US";

// This class holds various info about a language, as shown in the language
// settings list.
struct LanguageInfo {
  LanguageInfo();

  LanguageInfo(const LanguageInfo&);
  LanguageInfo(LanguageInfo&&) noexcept;
  LanguageInfo& operator=(const LanguageInfo&);
  LanguageInfo& operator=(LanguageInfo&&) noexcept;

  // This ISO code of the language.
  std::string code;
  // The display name of the language in the current locale.
  std::string display_name;
  // The display name of the language in the language locale.
  std::string native_display_name;
};

class LanguagePrefs {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit LanguagePrefs(PrefService* user_prefs);

  LanguagePrefs(const LanguagePrefs&) = delete;
  LanguagePrefs& operator=(const LanguagePrefs&) = delete;

  ~LanguagePrefs();

  // Gets the language settings list containing combination of policy-forced and
  // user-selected languages. Language settings list follows the Chrome internal
  // format.
  void GetAcceptLanguagesList(std::vector<std::string>* languages) const;
  // Gets the user-selected language settings list. Languages are expected to be
  // in the Chrome internal format.
  void GetUserSelectedLanguagesList(std::vector<std::string>* languages) const;
  // Updates the user-selected language settings list. Languages are expected to
  // be in the Chrome internal format.
  void SetUserSelectedLanguagesList(const std::vector<std::string>& languages);
  // Returns true if the target language is forced through policy.
  bool IsForcedLanguage(std::string_view language);
  // Get the languages that for which translation should never be prompted
  // formatted as Chrome language codes.
  std::vector<std::string> GetNeverTranslateLanguages() const;
  // Returns the last-observed translate target language.
  std::string GetRecentTargetLanguage() const;

  // This parameter specifies how the language should be moved within the list.
  enum RearrangeSpecifier {
    // No-op enumerator.
    kNone,
    // Move the language to the very top of the list.
    kTop,
    // Move the language up towards the front of the list.
    kUp,
    // Move the language down towards the back of the list.
    kDown
  };

  // Before adding to, removing from, or checking the fluent list the language
  // is converted to its translate synonym.
  bool IsBlockedLanguage(std::string_view language) const;
  void BlockLanguage(std::string_view language);
  void UnblockLanguage(std::string_view language);

  // Adds the language to the language list at chrome://settings/languages.
  // If the param |force_blocked| is set to true, the language is added to the
  // blocked list.
  // If force_blocked is set to false, the language is added to the blocked list
  // if the language list does not already contain another language with the
  // same base language.
  void AddToLanguageList(const base::i18n::LanguageTag& language_tag,
                         bool force_blocked);
  // Removes the language from the language list at chrome://settings/languages.
  void RemoveFromLanguageList(const base::i18n::LanguageTag& language_tag);

  // Rearranges the given language inside the language list.
  // The direction of the move is specified as a RearrangeSpecifier.
  // |offset| is ignored unless the RearrangeSpecifier is kUp or kDown: in
  // which case it needs to be positive for any change to be made.
  // The param |enabled_languages| is a list of languages that are enabled in
  // the current UI. This is required because the full language list contains
  // some languages that might not be enabled in the current UI and we need to
  // skip those languages while rearranging the list.
  void RearrangeLanguage(std::string_view language,
                         RearrangeSpecifier where,
                         int offset,
                         const std::vector<std::string>& enabled_languages);

  // Returns the list of LanguageInfo for all languages that are available in
  // the given locale. The list returned in |languages| is sorted
  // alphabetically based on the display names in the given locale.
  static void GetLanguageInfoList(const std::string& app_locale,
                                  std::vector<LanguageInfo>* languages);

#if BUILDFLAG(IS_ANDROID)
  // Get the ULP languages from a preference. This is an unfiltered list of
  // languages and may contain country specific language locales. If you do not
  // need specific locals always compare base languages from the list.
  std::vector<std::string> GetULPLanguages();
  // Clear the previous ULP language pref and set to the new list of languages.
  void SetULPLanguages(std::vector<base::i18n::LanguageTag> ulp_languages);
#endif

 private:
  // Returns the language settings list as parsed language tags.
  std::vector<base::i18n::LanguageTag> GetLanguageList() const;
  // Returns the user-selected language list as parsed language tags.
  std::vector<base::i18n::LanguageTag> GetUserSelectedLanguageList() const;
  // Updates the language list containing combination of policy-forced and
  // user-selected languages.
  void GetDeduplicatedUserLanguages(std::string* deduplicated_languages_string);
  // Updates the pref corresponding to the language list containing combination
  // of policy-forced and user-selected languages.
  // Since languages may be removed from the policy while the browser is off,
  // having an additional policy solely for user-selected languages allows
  // Chrome to clear any removed policy languages from the accept languages pref
  // while retaining all user-selected languages.
  void UpdateAcceptLanguagesPref();
  // Initializes the user selected language pref to ensure backwards
  // compatibility.
  void InitializeSelectedLanguagesPref();

  // Used for deduplication and reordering of languages.
  std::set<std::string, std::less<>> forced_languages_set_;

  raw_ptr<PrefService> prefs_;  // Weak.
  PrefChangeRegistrar pref_change_registrar_;
};

void ResetLanguagePrefs(PrefService* prefs);

// Given a comma separated list of locales, return the first.
std::string GetFirstLanguage(std::string_view language_list);

// Given a comma separated list of language tags, return a new list which keeps
// the same first language tag but otherwise matches one of Chrome's standard
// default configurations. This is to reduce the identifiability when incognito
// of users who've configured unusual language preferences.
std::string GetIncognitoLanguageList(std::string_view language_list);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
