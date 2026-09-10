// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_api.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/i18n/language_tag.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate_factory.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"

namespace extensions {

typedef api::language_settings_private::SpellcheckDictionaryStatus
    DictionaryStatus;

class MockLanguageSettingsPrivateDelegate
    : public LanguageSettingsPrivateDelegate {
 public:
  explicit MockLanguageSettingsPrivateDelegate(content::BrowserContext* context)
      : LanguageSettingsPrivateDelegate(context) {}
  ~MockLanguageSettingsPrivateDelegate() override = default;

  // LanguageSettingsPrivateDelegate:
  std::vector<DictionaryStatus> GetHunspellDictionaryStatuses() override;
  void RetryDownloadHunspellDictionary(const std::string& language) override;

  std::vector<std::string> retry_download_hunspell_dictionary_called_with() {
    return retry_download_hunspell_dictionary_called_with_;
  }

 private:
  std::vector<std::string> retry_download_hunspell_dictionary_called_with_;
};

std::vector<DictionaryStatus>
MockLanguageSettingsPrivateDelegate::GetHunspellDictionaryStatuses() {
  std::vector<DictionaryStatus> statuses;
  DictionaryStatus status;
  status.language_code = "fr";
  status.is_ready = false;
  status.is_downloading = true;
  status.download_failed = false;
  statuses.push_back(std::move(status));
  return statuses;
}

void MockLanguageSettingsPrivateDelegate::RetryDownloadHunspellDictionary(
    const std::string& language) {
  retry_download_hunspell_dictionary_called_with_.push_back(language);
}

namespace {

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<EventRouter>(profile, ExtensionPrefs::Get(profile));
}

std::unique_ptr<KeyedService> BuildLanguageSettingsPrivateDelegate(
    content::BrowserContext* profile) {
  return std::make_unique<MockLanguageSettingsPrivateDelegate>(profile);
}

std::unique_ptr<KeyedService> BuildSpellcheckService(
    content::BrowserContext* profile) {
  return std::make_unique<SpellcheckService>(static_cast<Profile*>(profile));
}

}  // namespace

class LanguageSettingsPrivateApiTest : public ExtensionServiceTestBase {
 public:
  LanguageSettingsPrivateApiTest() = default;
  ~LanguageSettingsPrivateApiTest() override = default;

 protected:
  void RunGetLanguageListTest();

 private:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceTestBase::InitializeEmptyExtensionService();
    EventRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildEventRouter));

    LanguageSettingsPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildLanguageSettingsPrivateDelegate));

    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating(&BuildSpellcheckService));
  }
};

TEST_F(LanguageSettingsPrivateApiTest, RetryDownloadHunspellDictionaryTest) {
  MockLanguageSettingsPrivateDelegate* mock_delegate =
      static_cast<MockLanguageSettingsPrivateDelegate*>(
          LanguageSettingsPrivateDelegateFactory::GetForBrowserContext(
              browser_context()));

  auto function = base::MakeRefCounted<
      LanguageSettingsPrivateRetryDownloadDictionaryFunction>();

  EXPECT_EQ(
      0u,
      mock_delegate->retry_download_hunspell_dictionary_called_with().size());
  EXPECT_TRUE(
      api_test_utils::RunFunction(function.get(), "[\"fr\"]", profile()))
      << function->GetError();
  EXPECT_EQ(
      1u,
      mock_delegate->retry_download_hunspell_dictionary_called_with().size());
  EXPECT_EQ(
      "fr",
      mock_delegate->retry_download_hunspell_dictionary_called_with().front());
}

TEST_F(LanguageSettingsPrivateApiTest, GetSpellcheckDictionaryStatusesTest) {
  auto function = base::MakeRefCounted<
      LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction>();

  std::optional<base::Value> actual =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       profile());
  ASSERT_TRUE(actual) << function->GetError();

  base::ListValue expected;
  base::DictValue expected_status;
  expected_status.Set("languageCode", "fr");
  expected_status.Set("isReady", false);
  expected_status.Set("isDownloading", true);
  expected_status.Set("downloadFailed", false);
  expected.Append(std::move(expected_status));
  EXPECT_EQ(base::Value(std::move(expected)), *actual);
}

TEST_F(LanguageSettingsPrivateApiTest, SetLanguageAlwaysTranslateStateTest) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_ =
      ChromeTranslateClient::CreateTranslatePrefs(profile()->GetPrefs());

  EXPECT_FALSE(translate_prefs_->HasLanguagePairsToAlwaysTranslate());

  auto function = base::MakeRefCounted<
      LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction>();
  api_test_utils::RunFunction(function.get(), "[\"af\", true]", profile());
  EXPECT_TRUE(translate_prefs_->HasLanguagePairsToAlwaysTranslate());

  function = base::MakeRefCounted<
      LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction>();
  api_test_utils::RunFunction(function.get(), "[\"af\", false]", profile());
  EXPECT_FALSE(translate_prefs_->HasLanguagePairsToAlwaysTranslate());
}

TEST_F(LanguageSettingsPrivateApiTest, GetAlwaysTranslateLanguagesListTest) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_ =
      ChromeTranslateClient::CreateTranslatePrefs(profile()->GetPrefs());

  EXPECT_FALSE(translate_prefs_->HasLanguagePairsToAlwaysTranslate());
  translate_prefs_->AddLanguagePairToAlwaysTranslateList("ak", "en");
  EXPECT_TRUE(translate_prefs_->HasLanguagePairsToAlwaysTranslate());

  translate_prefs_->AddLanguagePairToAlwaysTranslateList("af", "es");
  translate_prefs_->AddLanguagePairToAlwaysTranslateList("fil", "es");
  std::vector<std::string> always_translate_languages =
      translate_prefs_->GetAlwaysTranslateLanguages();
  ASSERT_EQ(std::vector<std::string>({"af", "ak", "fil"}),
            always_translate_languages);

  auto function = base::MakeRefCounted<
      LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction>();

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       profile());

  ASSERT_TRUE(result) << function->GetError();
  ASSERT_TRUE(result->is_list());

  ASSERT_EQ(result->GetList().size(), always_translate_languages.size());
  for (size_t i = 0; i < result->GetList().size(); i++) {
    EXPECT_EQ(result->GetList()[i].GetString(), always_translate_languages[i]);
  }
}

TEST_F(LanguageSettingsPrivateApiTest, SetTranslateTargetLanguageTest) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_ =
      ChromeTranslateClient::CreateTranslatePrefs(profile()->GetPrefs());

  std::vector<base::i18n::LanguageTag> content_languages_tags =
      translate_prefs_->GetLanguageList();
  std::vector<std::string> content_languages_before;
  for (const auto& tag : content_languages_tags) {
    content_languages_before.push_back(std::string(tag.tag_string()));
  }

  ASSERT_EQ(std::vector<std::string>({"en-US", "en"}),
            content_languages_before);
  translate_prefs_->SetRecentTargetLanguage("en");
  ASSERT_EQ(translate_prefs_->GetRecentTargetLanguage(), "en");

  auto function = base::MakeRefCounted<
      LanguageSettingsPrivateSetTranslateTargetLanguageFunction>();

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(),
                                                       "[\"af\"]", profile());
  ASSERT_EQ(translate_prefs_->GetRecentTargetLanguage(), "af");
}

TEST_F(LanguageSettingsPrivateApiTest, GetNeverTranslateLanguagesListTest) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_ =
      ChromeTranslateClient::CreateTranslatePrefs(profile()->GetPrefs());

  std::vector<std::string> never_translate_languages =
      translate_prefs_->GetNeverTranslateLanguages();
  ASSERT_EQ(std::vector<std::string>({"en"}), never_translate_languages);
  translate_prefs_->BlockLanguage("af");
  translate_prefs_->BlockLanguage("es");
  never_translate_languages = translate_prefs_->GetNeverTranslateLanguages();
  ASSERT_EQ(std::vector<std::string>({"en", "af", "es"}),
            never_translate_languages);

  auto function = base::MakeRefCounted<
      LanguageSettingsPrivateGetNeverTranslateLanguagesFunction>();

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       profile());

  ASSERT_TRUE(result) << function->GetError();
  ASSERT_TRUE(result->is_list());

  ASSERT_EQ(result->GetList().size(), never_translate_languages.size());
  for (size_t i = 0; i < result->GetList().size(); i++) {
    EXPECT_EQ(result->GetList()[i].GetString(), never_translate_languages[i]);
  }
}

void LanguageSettingsPrivateApiTest::RunGetLanguageListTest() {
  struct LanguageToTest {
    std::string accept_language;
    std::string windows_dictionary_name;  // Empty string indicates to not use
                                          // fake Windows dictionary
    bool is_preferred_language;
    bool is_spellcheck_support_expected;
  };

  std::vector<LanguageToTest> languages_to_test = {
      // Languages with both Windows and Hunspell spellcheck support.
      // GetLanguageList should always report spellchecking to be supported for
      // these languages, regardless of whether a language pack is installed or
      // if it is a preferred language.
      {"fr", "fr-FR", true, true},
      {"de", "de-DE", false, true},
      {"es-MX", "", true, true},
      {"fa", "", false, true},
      {"gl", "", true, true},
      {"zu", "", false, false},
      // Finnish with Filipino language pack (string in string).
      {"fi", "fil", true, false},
      // Sesotho with Asturian language pack (string in string).
      {"st", "ast", true, false},
  };

  languages_to_test.push_back({"ar", "ar-SA", true, false});
  languages_to_test.push_back({"bn", "bn-IN", false, false});

  // Initialize accept languages prefs.
  std::vector<std::string> accept_languages;
  for (auto& language_to_test : languages_to_test) {
    if (language_to_test.is_preferred_language) {
      accept_languages.push_back(language_to_test.accept_language);
    }
  }

  std::string accept_languages_string = base::JoinString(accept_languages, ",");
  DVLOG(2) << "Setting accept languages preferences to: "
           << accept_languages_string;
  profile()->GetPrefs()->SetString(language::prefs::kAcceptLanguages,
                                   accept_languages_string);

  auto function =
      base::MakeRefCounted<LanguageSettingsPrivateGetLanguageListFunction>();

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       profile());

  ASSERT_TRUE(result) << function->GetError();
  ASSERT_TRUE(result->is_list());

  size_t languages_to_test_found_count = 0;
  for (auto& language_val : result->GetList()) {
    EXPECT_TRUE(language_val.is_dict());
    std::string* language_code_ptr = language_val.GetDict().FindString("code");
    ASSERT_NE(nullptr, language_code_ptr);
    std::string language_code = *language_code_ptr;
    EXPECT_FALSE(language_code.empty());

    const std::optional<bool> maybe_supports_spellcheck =
        language_val.GetDict().FindBool("supportsSpellcheck");
    const bool supports_spellcheck = maybe_supports_spellcheck.has_value()
                                         ? maybe_supports_spellcheck.value()
                                         : false;

    for (auto& language_to_test : languages_to_test) {
      if (language_to_test.accept_language == language_code) {
        DVLOG(2) << "*** Found language code being tested=" << language_code
                 << ", supportsSpellcheck=" << supports_spellcheck << " ***";
        EXPECT_EQ(language_to_test.is_spellcheck_support_expected,
                  supports_spellcheck);
        languages_to_test_found_count++;
        break;
      }
    }

    // Check that zh and zh-HK aren't shown as supporting UI.
    if (language_code == "zh" || language_code == "zh-HK") {
      const std::optional<bool> maybe_supports_ui =
          language_val.GetDict().FindBool("supportsUI");
      const bool supports_ui =
          maybe_supports_ui.has_value() ? maybe_supports_ui.value() : false;
      EXPECT_FALSE(supports_ui) << language_code << " should not support UI";
    }
  }

  EXPECT_EQ(languages_to_test.size(), languages_to_test_found_count);
}

}  // namespace extensions
