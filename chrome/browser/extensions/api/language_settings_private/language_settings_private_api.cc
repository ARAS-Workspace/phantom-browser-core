// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_api.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/language_settings_private.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "extensions/browser/extensions_browser_client.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate_factory.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#endif

namespace extensions {

namespace language_settings_private = api::language_settings_private;

namespace {

std::unique_ptr<language::LanguagePrefs> CreateLanguagePrefsForBrowserContext(
    content::BrowserContext* browser_context) {
  return std::make_unique<language::LanguagePrefs>(
      Profile::FromBrowserContext(browser_context)->GetPrefs());
}

}  // namespace

LanguageSettingsPrivateGetLanguageListFunction::
    LanguageSettingsPrivateGetLanguageListFunction() = default;

LanguageSettingsPrivateGetLanguageListFunction::
    ~LanguageSettingsPrivateGetLanguageListFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetLanguageListFunction::Run() {
  // Collect the language codes from the supported accept-languages.
  const std::string app_locale =
      ExtensionsBrowserClient::Get()->GetApplicationLocale();
  std::vector<language::LanguageInfo> languages;
  language::LanguagePrefs::GetLanguageInfoList(app_locale, &languages);

  // Get the list of spell check languages and convert to a set.
#if BUILDFLAG(ENABLE_SPELLCHECK)
  std::vector<std::string> spellcheck_languages =
      spellcheck::SpellCheckLanguages();
#else
  std::vector<std::string> spellcheck_languages;
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
  const base::flat_set<std::string> spellcheck_language_set(
      std::move(spellcheck_languages));

  // Build the language list.
  language_list_.clear();
  for (const auto& entry : languages) {
    language_settings_private::Language language;

    language.code = entry.code;
    language.display_name = entry.display_name;
    language.native_display_name = entry.native_display_name;

    // Set optional fields only if they differ from the default.
    if (spellcheck_language_set.contains(entry.code)) {
      language.supports_spellcheck = true;
    }

    if (l10n_util::IsUserFacingUILocale(entry.code)) {
      language.supports_ui = true;
    }

    language_list_.Append(language.ToValue());
  }

  return RespondNow(WithArguments(std::move(language_list_)));
}

LanguageSettingsPrivateEnableLanguageFunction::
    LanguageSettingsPrivateEnableLanguageFunction() = default;

LanguageSettingsPrivateEnableLanguageFunction::
    ~LanguageSettingsPrivateEnableLanguageFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateEnableLanguageFunction::Run() {
  const auto parameters =
      language_settings_private::EnableLanguage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  const std::string& language_code = parameters->language_code;

  std::unique_ptr<language::LanguagePrefs> language_prefs =
      CreateLanguagePrefsForBrowserContext(browser_context());

  if (std::optional<base::i18n::LanguageTag> parsed_tag =
          base::i18n::LanguageTagConverter::GetInstance().FromString(
              language_code)) {
    language_prefs->AddToLanguageList(*parsed_tag, /*force_blocked=*/false);
  }

  return RespondNow(NoArguments());
}

LanguageSettingsPrivateDisableLanguageFunction::
    LanguageSettingsPrivateDisableLanguageFunction() = default;

LanguageSettingsPrivateDisableLanguageFunction::
    ~LanguageSettingsPrivateDisableLanguageFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateDisableLanguageFunction::Run() {
  const auto parameters =
      language_settings_private::DisableLanguage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  const std::string& language_code = parameters->language_code;

  std::unique_ptr<language::LanguagePrefs> language_prefs =
      CreateLanguagePrefsForBrowserContext(browser_context());

  if (std::optional<base::i18n::LanguageTag> parsed_tag =
          base::i18n::LanguageTagConverter::GetInstance().FromString(
              language_code)) {
    language_prefs->RemoveFromLanguageList(*parsed_tag);
  }

  return RespondNow(NoArguments());
}

LanguageSettingsPrivateMoveLanguageFunction::
    LanguageSettingsPrivateMoveLanguageFunction() = default;

LanguageSettingsPrivateMoveLanguageFunction::
    ~LanguageSettingsPrivateMoveLanguageFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateMoveLanguageFunction::Run() {
  const auto parameters =
      language_settings_private::MoveLanguage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  const std::string app_locale =
      ExtensionsBrowserClient::Get()->GetApplicationLocale();
  std::vector<std::string> supported_language_codes =
      l10n_util::GetAcceptLanguagesForLocale(app_locale);

  const std::string& language_code = parameters->language_code;
  const language_settings_private::MoveType move_type = parameters->move_type;

  std::unique_ptr<language::LanguagePrefs> language_prefs =
      CreateLanguagePrefsForBrowserContext(browser_context());

  language::LanguagePrefs::RearrangeSpecifier where =
      language::LanguagePrefs::kNone;
  switch (move_type) {
    case language_settings_private::MoveType::kTop:
      where = language::LanguagePrefs::kTop;
      break;

    case language_settings_private::MoveType::kUp:
      where = language::LanguagePrefs::kUp;
      break;

    case language_settings_private::MoveType::kDown:
      where = language::LanguagePrefs::kDown;
      break;

    case language_settings_private::MoveType::kNone:
    case language_settings_private::MoveType::kMaxValue:
      NOTREACHED();
  }

  // On Desktop we can only move languages by one position.
  const int offset = 1;
  language_prefs->RearrangeLanguage(language_code, where, offset,
                                    supported_language_codes);

  return RespondNow(NoArguments());
}

LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::
    LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction() = default;

LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::
    ~LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::Run() {
#if BUILDFLAG(ENABLE_SPELLCHECK)
  LanguageSettingsPrivateDelegate* delegate =
      LanguageSettingsPrivateDelegateFactory::GetForBrowserContext(
          browser_context());

  return RespondNow(ArgumentList(
      language_settings_private::GetSpellcheckDictionaryStatuses::Results::
          Create(delegate->GetHunspellDictionaryStatuses())));
#else
  return RespondNow(Error("Spell check is not available in this build."));
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
}

LanguageSettingsPrivateGetSpellcheckWordsFunction::
    LanguageSettingsPrivateGetSpellcheckWordsFunction() = default;

LanguageSettingsPrivateGetSpellcheckWordsFunction::
    ~LanguageSettingsPrivateGetSpellcheckWordsFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetSpellcheckWordsFunction::Run() {
#if BUILDFLAG(ENABLE_SPELLCHECK)
  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  SpellcheckCustomDictionary* dictionary = service->GetCustomDictionary();

  if (dictionary->IsLoaded()) {
    return RespondNow(WithArguments(GetSpellcheckWords()));
  }

  dictionary->AddObserver(this);
  AddRef();  // Balanced in OnCustomDictionaryLoaded().
  return RespondLater();
#else
  return RespondNow(Error("Spell check is not available in this build."));
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
}

#if BUILDFLAG(ENABLE_SPELLCHECK)
void LanguageSettingsPrivateGetSpellcheckWordsFunction::
    OnCustomDictionaryLoaded() {
  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  service->GetCustomDictionary()->RemoveObserver(this);
  Respond(WithArguments(GetSpellcheckWords()));
  Release();
}

void LanguageSettingsPrivateGetSpellcheckWordsFunction::
    OnCustomDictionaryChanged(
        const SpellcheckCustomDictionary::Change& dictionary_change) {
  NOTREACHED()
      << "SpellcheckCustomDictionary::Observer: OnCustomDictionaryChanged() "
         "called before OnCustomDictionaryLoaded()";
}

base::ListValue
LanguageSettingsPrivateGetSpellcheckWordsFunction::GetSpellcheckWords() const {
  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  SpellcheckCustomDictionary* dictionary = service->GetCustomDictionary();
  DCHECK(dictionary->IsLoaded());

  // TODO(michaelpg): Sort using app locale.
  base::ListValue word_list;
  std::set<std::string> words = dictionary->GetWords();
  word_list.reserve(words.size());
  for (auto it = words.begin(); it != words.end();) {
    word_list.Append(std::move(words.extract(it++).value()));
  }
  return word_list;
}
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

LanguageSettingsPrivateAddSpellcheckWordFunction::
    LanguageSettingsPrivateAddSpellcheckWordFunction() = default;

LanguageSettingsPrivateAddSpellcheckWordFunction::
    ~LanguageSettingsPrivateAddSpellcheckWordFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateAddSpellcheckWordFunction::Run() {
#if BUILDFLAG(ENABLE_SPELLCHECK)
  const auto params =
      language_settings_private::AddSpellcheckWord::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  bool success = service->GetCustomDictionary()->AddWord(params->word);

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (spellcheck::UseBrowserSpellChecker()) {
    spellcheck_platform::AddWord(service->platform_spell_checker(),
                                 base::UTF8ToUTF16(params->word));
  }
#endif

  return RespondNow(WithArguments(success));
#else
  return RespondNow(Error("Spell check is not available in this build."));
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
}

LanguageSettingsPrivateRemoveSpellcheckWordFunction::
    LanguageSettingsPrivateRemoveSpellcheckWordFunction() = default;

LanguageSettingsPrivateRemoveSpellcheckWordFunction::
    ~LanguageSettingsPrivateRemoveSpellcheckWordFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateRemoveSpellcheckWordFunction::Run() {
#if BUILDFLAG(ENABLE_SPELLCHECK)
  const auto params =
      language_settings_private::RemoveSpellcheckWord::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  bool success = service->GetCustomDictionary()->RemoveWord(params->word);

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (spellcheck::UseBrowserSpellChecker()) {
    spellcheck_platform::RemoveWord(service->platform_spell_checker(),
                                    base::UTF8ToUTF16(params->word));
  }
#endif

  return RespondNow(WithArguments(success));
#else
  return RespondNow(Error("Spell check is not available in this build."));
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
}

LanguageSettingsPrivateGetInputMethodListsFunction::
    LanguageSettingsPrivateGetInputMethodListsFunction() = default;

LanguageSettingsPrivateGetInputMethodListsFunction::
    ~LanguageSettingsPrivateGetInputMethodListsFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetInputMethodListsFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(false);
  return RespondNow(NoArguments());
}

LanguageSettingsPrivateAddInputMethodFunction::
    LanguageSettingsPrivateAddInputMethodFunction() = default;

LanguageSettingsPrivateAddInputMethodFunction::
    ~LanguageSettingsPrivateAddInputMethodFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateAddInputMethodFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(false);
  return RespondNow(NoArguments());
}

LanguageSettingsPrivateRemoveInputMethodFunction::
    LanguageSettingsPrivateRemoveInputMethodFunction() = default;

LanguageSettingsPrivateRemoveInputMethodFunction::
    ~LanguageSettingsPrivateRemoveInputMethodFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateRemoveInputMethodFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(false);
  return RespondNow(NoArguments());
}

LanguageSettingsPrivateRetryDownloadDictionaryFunction::
    LanguageSettingsPrivateRetryDownloadDictionaryFunction() = default;

LanguageSettingsPrivateRetryDownloadDictionaryFunction::
    ~LanguageSettingsPrivateRetryDownloadDictionaryFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateRetryDownloadDictionaryFunction::Run() {
#if BUILDFLAG(ENABLE_SPELLCHECK)
  const auto parameters =
      language_settings_private::RetryDownloadDictionary::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  LanguageSettingsPrivateDelegate* delegate =
      LanguageSettingsPrivateDelegateFactory::GetForBrowserContext(
          browser_context());
  delegate->RetryDownloadHunspellDictionary(parameters->language_code);
  return RespondNow(NoArguments());
#else
  return RespondNow(Error("Spell check is not available in this build."));
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
}

}  // namespace extensions
