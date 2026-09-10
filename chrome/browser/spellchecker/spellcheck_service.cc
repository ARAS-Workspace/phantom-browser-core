// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"
#include "chrome/common/chrome_features.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/browser/spelling_service_client.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

SpellcheckService::SpellCheckerBinder& GetSpellCheckerBinderOverride() {
  static base::NoDestructor<SpellcheckService::SpellCheckerBinder> binder;
  return *binder;
}

// Only record spelling-configuration metrics for profiles in which the user
// can configure spelling.
bool RecordSpellingConfigurationMetrics(content::BrowserContext* context) {
  return profiles::IsRegularUserProfile(Profile::FromBrowserContext(context));
}

}  // namespace

// TODO(rlp): I do not like globals, but keeping these for now during
// transition.
// An event used by browser tests to receive status events from this class and
// its derived classes.
base::WaitableEvent* g_status_event = nullptr;
SpellcheckService::EventType g_status_type =
    SpellcheckService::BDICT_NOTINITIALIZED;

SpellcheckService::SpellcheckService(content::BrowserContext* context)
    : context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool defer_spellcheck =
      base::FeatureList::IsEnabled(::features::kDeferSpellcheckInitialization);

  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  pref_change_registrar_.Init(prefs);
  StringListPrefMember dictionaries_pref;
  dictionaries_pref.Init(spellcheck::prefs::kSpellCheckDictionaries, prefs);
  std::string first_of_dictionaries;

#if BUILDFLAG(IS_MAC)
  if (defer_spellcheck) {
    // Defer Cocoa spellchecker initialization to a post-startup idle task to
    // avoid blocking the main thread during critical startup path.
    // This is safe because:
    // 1. Spellcheck is not needed immediately on startup.
    // 2. Renderers that don't need spellcheck (like Top Chrome WebUI and NTP)
    //    are bypassed entirely in `InitForRenderer`, so they won't trigger
    //    early initialization of the service.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&spellcheck_platform::GetSpellCheckerLanguage),
        base::BindOnce(&SpellcheckService::InitMacDeferredSpellcheck,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    InitializePlatformLanguageMacWithLanguage(
        spellcheck_platform::GetSpellCheckerLanguage());
  }
#elif BUILDFLAG(IS_ANDROID)
  // Ensure that the renderer always knows the platform spellchecking
  // language. This language is used for initialization of the text iterator.
  // If the iterator is not initialized, then the context menu does not show
  // spellcheck suggestions.
  dictionaries_pref.SetValue(std::vector<std::string>(
      1, spellcheck_platform::GetSpellCheckerLanguage()));
#else
  // Migrate preferences from single-language to multi-language schema.
  StringPrefMember single_dictionary_pref;
  single_dictionary_pref.Init(spellcheck::prefs::kSpellCheckDictionary, prefs);
  std::string single_dictionary = single_dictionary_pref.GetValue();

  if (!dictionaries_pref.GetValue().empty())
    first_of_dictionaries = dictionaries_pref.GetValue().front();

  if (first_of_dictionaries.empty() && !single_dictionary.empty()) {
    first_of_dictionaries = single_dictionary;
    dictionaries_pref.SetValue(
        std::vector<std::string>(1, first_of_dictionaries));
  }

  single_dictionary_pref.SetValue("");
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckDictionaries,
      base::BindRepeating(&SpellcheckService::OnSpellCheckDictionariesChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckForcedDictionaries,
      base::BindRepeating(&SpellcheckService::OnSpellCheckDictionariesChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckBlocklistedDictionaries,
      base::BindRepeating(&SpellcheckService::OnSpellCheckDictionariesChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckUseSpellingService,
      base::BindRepeating(&SpellcheckService::OnUseSpellingServiceChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      language::prefs::kAcceptLanguages,
      base::BindRepeating(&SpellcheckService::OnAcceptLanguagesChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckEnable,
      base::BindRepeating(&SpellcheckService::InitForAllRenderers,
                          base::Unretained(this)));

  custom_dictionary_ =
      std::make_unique<SpellcheckCustomDictionary>(context_->GetPath());
  custom_dictionary_->AddObserver(this);

  // Determine if dictionary initialization should be deferred or skipped in
  // this constructor.
  bool run_custom_dict_load = true;
  bool run_hunspell_init = true;

#if BUILDFLAG(IS_MAC)
  if (defer_spellcheck) {
    run_custom_dict_load = false;
    run_hunspell_init = false;
  }
#endif

  // 1. Load custom dictionary.
  if (run_custom_dict_load) {
    if (defer_spellcheck) {
      content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&SpellcheckCustomDictionary::Load,
                                    custom_dictionary_->GetWeakPtr()));
    } else {
      custom_dictionary_->Load();
    }
  }

  // 2. Initialize Hunspell dictionaries.
  if (run_hunspell_init) {
    if (defer_spellcheck) {
      content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&SpellcheckService::InitializeDictionaries,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    base::DoNothing()));
    } else {
      InitializeDictionaries(base::DoNothing());
    }
  }
}

SpellcheckService::~SpellcheckService() {
  // Remove pref observers
  pref_change_registrar_.RemoveAll();
}

base::WeakPtr<SpellcheckService> SpellcheckService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

#if BUILDFLAG(IS_MAC)
void SpellcheckService::InitializePlatformLanguageMacWithLanguage(
    const std::string& platform_lang) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  StringListPrefMember dictionaries_pref;
  dictionaries_pref.Init(spellcheck::prefs::kSpellCheckDictionaries, prefs);

  // Ensure that the renderer always knows the platform spellchecking
  // language. This language is used for initialization of the text iterator.
  // If the iterator is not initialized, then the context menu does not show
  // spellcheck suggestions.
  // No migration is necessary, because the spellcheck language preference is
  // not user visible or modifiable in Chrome on Mac.
  if (dictionaries_pref.GetValue().empty() ||
      dictionaries_pref.GetValue().front() != platform_lang) {
    dictionaries_pref.SetValue(std::vector<std::string>(1, platform_lang));
  }
}

// InitMacDeferredSpellcheck orchestrates the startup sequence for Mac
// spellcheck when deferred initialization is enabled.
//
// The initialization flow works as follows:
// 1. Get the system's preferred spellchecker language off-thread (called via
//    ThreadPool::PostTaskAndReplyWithResult in the constructor). This is
//    done because querying the platform language can block the main thread.
// 2. Once the language is resolved, InitMacDeferredSpellcheck is run on the
//    UI thread.
// 3. Initialize Hunspell dictionaries first. This sets up the dictionary
//    infrastructure and StartRecordingMetrics() so metrics are not dropped.
// 4. Initialize platform language. If the preference changes, the preference
//    observer will trigger LoadDictionaries() to perform the actual load.
// 5. Load the custom dictionary database asynchronously.
void SpellcheckService::InitMacDeferredSpellcheck(
    const std::string& platform_lang) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // 1. Initialize Hunspell dictionaries first to set up metrics.
  InitializeDictionaries(base::DoNothing());

  // 2. Initialize platform language. If it updates the preference, the
  // observer will automatically call LoadDictionaries() again.
  InitializePlatformLanguageMacWithLanguage(platform_lang);

  // 3. Load custom dictionary database.
  custom_dictionary_->Load();
}
#endif

#if !BUILDFLAG(IS_MAC)
// static
void SpellcheckService::GetDictionaries(
    content::BrowserContext* browser_context,
    std::vector<Dictionary>* dictionaries) {
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
  absl::flat_hash_set<std::string> spellcheck_dictionaries;
  for (const auto& value :
       prefs->GetList(spellcheck::prefs::kSpellCheckDictionaries)) {
    const std::string* dictionary = value.GetIfString();
    if (dictionary)
      spellcheck_dictionaries.insert(*dictionary);
  }

  dictionaries->clear();
  std::vector<std::string> accept_languages =
      base::SplitString(prefs->GetString(language::prefs::kAcceptLanguages),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& accept_language : accept_languages) {
    Dictionary dictionary;
    dictionary.language =
        spellcheck::GetCorrespondingSpellCheckLanguage(accept_language);

    if (dictionary.language.empty())
      continue;

    dictionary.used_for_spellcheck =
        spellcheck_dictionaries.contains(dictionary.language);
    dictionaries->push_back(dictionary);
  }
}
#endif  // !BUILDFLAG(IS_MAC)

// static
bool SpellcheckService::SignalStatusEvent(
    SpellcheckService::EventType status_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!g_status_event)
    return false;
  g_status_type = status_type;
  g_status_event->Signal();
  return true;
}

// static
std::string SpellcheckService::GetSupportedAcceptLanguageCode(
    const std::string& supported_language_full_tag,
    bool generic_only) {
  // Default to accept language in hardcoded list of Hunspell dictionaries
  // (kSupportedSpellCheckerLanguages).
  std::string supported_accept_language =
      spellcheck::GetCorrespondingSpellCheckLanguage(
          supported_language_full_tag);
  if (generic_only) {
    supported_accept_language = SpellcheckService::GetLanguageAndScriptTag(
        supported_accept_language,
        /* include_script_tag= */ false);
  }

  return supported_accept_language;
}

void SpellcheckService::StartRecordingMetrics(bool spellcheck_enabled) {
  metrics_ = std::make_unique<SpellCheckHostMetrics>();
  auto record_configuration_metrics =
      RecordSpellingConfigurationMetrics(context_);
  if (record_configuration_metrics) {
    metrics_->RecordEnabledStats(spellcheck_enabled);
  }

  OnUseSpellingServiceChanged();

}

void SpellcheckService::InitForRenderer(content::RenderProcessHost* host) {
  // Skip initialization of the spellcheck service for top chrome and NTP web UI
  // pages when Initial WebUI feature is enabled for optimizing browser startup.
  if (host->IsForTopChromeWebUI() &&
      base::FeatureList::IsEnabled(features::kInitialWebUI) &&
      features::kInitialWebUIWithoutSpellCheck.Get()) {
    return;
  }
  if (base::FeatureList::IsEnabled(
          features::kInitialWebUIWithoutSpellCheckForNtp) &&
      host->GetUserData(search::kIsNTPProcessKey)) {
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* context = host->GetBrowserContext();
  if (SpellcheckServiceFactory::GetForContext(context) != this)
    return;

  const bool enable = IsSpellcheckEnabled();

  std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries;
  std::vector<std::string> custom_words;
  if (enable) {
    for (const auto& hunspell_dictionary : hunspell_dictionaries_) {
      dictionaries.push_back(spellcheck::mojom::SpellCheckBDictLanguage::New(
          hunspell_dictionary->GetDictionaryFile().Duplicate(),
          hunspell_dictionary->GetLanguage()));
    }

    std::set<std::string> custom_words_set = custom_dictionary_->GetWords();
    custom_words.assign(std::make_move_iterator(custom_words_set.begin()),
                        std::make_move_iterator(custom_words_set.end()));
  } else {
    // Disabling spell check should also disable spelling service.
    user_prefs::UserPrefs::Get(context)->SetBoolean(
        spellcheck::prefs::kSpellCheckUseSpellingService, false);
  }

  GetSpellCheckerForProcess(host)->Initialize(std::move(dictionaries),
                                              custom_words, enable);
}

SpellCheckHostMetrics* SpellcheckService::GetMetrics() const {
  return metrics_.get();
}

SpellcheckCustomDictionary* SpellcheckService::GetCustomDictionary() {
  return custom_dictionary_.get();
}

void SpellcheckService::LoadDictionaries() {
  hunspell_dictionaries_.clear();

  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  DCHECK(prefs);

  const base::ListValue& user_dictionaries =
      prefs->GetList(spellcheck::prefs::kSpellCheckDictionaries);
  const base::ListValue& forced_dictionaries =
      prefs->GetList(spellcheck::prefs::kSpellCheckForcedDictionaries);

  // Build a lookup of blocked dictionaries to skip loading them.
  const base::ListValue& blocked_dictionaries =
      prefs->GetList(spellcheck::prefs::kSpellCheckBlocklistedDictionaries);
  absl::flat_hash_set<std::string_view> blocked_dictionaries_lookup;
  blocked_dictionaries_lookup.reserve(blocked_dictionaries.size());
  for (const auto& blocked_dict : blocked_dictionaries) {
    blocked_dictionaries_lookup.insert(blocked_dict.GetString());
  }

  // Merge both lists of dictionaries. Use a set to avoid duplicates.
  std::set<std::string> dictionaries;
  for (const auto& dictionary_value : user_dictionaries) {
    if (!blocked_dictionaries_lookup.contains(dictionary_value.GetString())) {
      dictionaries.insert(dictionary_value.GetString());
    }
  }
  for (const auto& dictionary_value : forced_dictionaries) {
    dictionaries.insert(dictionary_value.GetString());
  }

  for (const std::string& dictionary : dictionaries) {
    // The spellcheck language passed to platform APIs may differ from the
    // accept language.
    std::string platform_spellcheck_language;

    hunspell_dictionaries_.push_back(
        std::make_unique<SpellcheckHunspellDictionary>(
            dictionary, platform_spellcheck_language, context_, this));
    hunspell_dictionaries_.back()->AddObserver(this);
    hunspell_dictionaries_.back()->Load();
  }

  dictionaries_loaded_ = true;
}

const std::vector<std::unique_ptr<SpellcheckHunspellDictionary>>&
SpellcheckService::GetHunspellDictionaries() {
  return hunspell_dictionaries_;
}

bool SpellcheckService::IsSpellcheckEnabled() const {
  const PrefService* prefs = user_prefs::UserPrefs::Get(context_);

  bool enable_if_uninitialized = false;

  return prefs->GetBoolean(spellcheck::prefs::kSpellCheckEnable) &&
         (!hunspell_dictionaries_.empty() || enable_if_uninitialized);
}

void SpellcheckService::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  InitForRenderer(host);
}

void SpellcheckService::OnCustomDictionaryLoaded() {
  InitForAllRenderers();
}

void SpellcheckService::OnCustomDictionaryChanged(
    const SpellcheckCustomDictionary::Change& change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto additions = base::ToVector(change.to_add());
  const auto deletions = base::ToVector(change.to_remove());
  for (auto it = content::RenderProcessHost::AllHostsIterator(); !it.IsAtEnd();
       it.Advance()) {
    content::RenderProcessHost* process = it.GetCurrentValue();
    if (!process->IsInitializedAndNotDead() ||
        SpellcheckServiceFactory::GetForContext(process->GetBrowserContext()) !=
            this) {
      continue;
    }

    GetSpellCheckerForProcess(process)->CustomDictionaryChanged(additions,
                                                                deletions);
  }
}

void SpellcheckService::OnHunspellDictionaryInitialized(
    const std::string& language) {
  InitForAllRenderers();
}

void SpellcheckService::OnHunspellDictionaryDownloadBegin(
    const std::string& language) {
}

void SpellcheckService::OnHunspellDictionaryDownloadSuccess(
    const std::string& language) {
}

void SpellcheckService::OnHunspellDictionaryDownloadFailure(
    const std::string& language) {
}

void SpellcheckService::InitializeDictionaries(base::OnceClosure done) {
  // The dictionaries only need to be initialized once.
  if (dictionaries_loaded()) {
    std::move(done).Run();
    return;
  }

  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  DCHECK(prefs);

  // Instantiates Metrics object for spellchecking to use.
  StartRecordingMetrics(
      prefs->GetBoolean(spellcheck::prefs::kSpellCheckEnable));

  // Using Hunspell.
  LoadDictionaries();
}

// static
void SpellcheckService::OverrideBinderForTesting(SpellCheckerBinder binder) {
  GetSpellCheckerBinderOverride() = std::move(binder);
}

// static
std::string SpellcheckService::GetLanguageAndScriptTag(
    const std::string& full_tag,
    bool include_script_tag) {
  if (full_tag.empty())
    return "";

  std::string language_and_script_tag;

  std::vector<std::string> subtags = base::SplitString(
      full_tag, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // Language subtag is required, all others optional.
  DCHECK_GE(subtags.size(), 1ULL);
  std::vector<std::string> subtag_tokens_to_pass;
  subtag_tokens_to_pass.push_back(subtags.front());
  subtags.erase(subtags.begin());

  // The optional script subtag always follows the language subtag, and is 4
  // characters in length.
  if (include_script_tag) {
    if (!subtags.empty() && subtags.front().length() == 4) {
      subtag_tokens_to_pass.push_back(subtags.front());
    }
  }

  language_and_script_tag = base::JoinString(subtag_tokens_to_pass, "-");

  return language_and_script_tag;
}

// static
void SpellcheckService::AttachStatusEvent(base::WaitableEvent* status_event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  g_status_event = status_event;
}

// static
SpellcheckService::EventType SpellcheckService::GetStatusEvent() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_status_type;
}

mojo::Remote<spellcheck::mojom::SpellChecker>
SpellcheckService::GetSpellCheckerForProcess(content::RenderProcessHost* host) {
  mojo::Remote<spellcheck::mojom::SpellChecker> spellchecker;
  auto receiver = spellchecker.BindNewPipeAndPassReceiver();
  auto binder = GetSpellCheckerBinderOverride();
  if (binder)
    binder.Run(std::move(receiver));
  else
    host->BindReceiver(std::move(receiver));
  return spellchecker;
}

void SpellcheckService::InitForAllRenderers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto it = content::RenderProcessHost::AllHostsIterator(); !it.IsAtEnd();
       it.Advance()) {
    content::RenderProcessHost* process = it.GetCurrentValue();
    // Do not test GetProcess().Handle() here: a renderer that has been
    // initialized but whose process is still launching has no handle yet, and
    // skipping it drops the notification for good. Its mojo channel already
    // exists, so messages queue up and are delivered once the process is up.
    // This matches OnCustomDictionaryChanged() below.
    if (process && process->IsInitializedAndNotDead()) {
      InitForRenderer(process);
    }
  }
}

void SpellcheckService::OnSpellCheckDictionariesChanged() {
  // If there are hunspell dictionaries, then fire off notifications to the
  // renderers after the dictionaries are finished loading.
  LoadDictionaries();

  // If there are no hunspell dictionaries to load, then immediately let the
  // renderers know the new state.
  if (hunspell_dictionaries_.empty()) {
#if !BUILDFLAG(IS_MAC)
    // Only update non-MacOS platform because basic spell check on Mac OS
    // is controlled by OS and doesn't depend on users' dictionaries pref
    user_prefs::UserPrefs::Get(context_)->SetBoolean(
        spellcheck::prefs::kSpellCheckEnable, false);
#endif  // !BUILDFLAG(IS_MAC)
    InitForAllRenderers();
  }
}

void SpellcheckService::OnUseSpellingServiceChanged() {
  bool enabled = pref_change_registrar_.prefs()->GetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService);
  if (metrics_ && RecordSpellingConfigurationMetrics(context_)) {
    metrics_->RecordSpellingServiceStats(enabled);
  }
}

void SpellcheckService::OnAcceptLanguagesChanged() {
  // Accept-Languages and spell check are decoupled on CrOS.
  std::vector<std::string> accept_languages = GetNormalizedAcceptLanguages();

  StringListPrefMember dictionaries_pref;
  dictionaries_pref.Init(spellcheck::prefs::kSpellCheckDictionaries,
                         user_prefs::UserPrefs::Get(context_));
  std::vector<std::string> dictionaries = dictionaries_pref.GetValue();
  std::vector<std::string> filtered_dictionaries;

  for (const auto& dictionary : dictionaries) {
    if (std::ranges::contains(accept_languages, dictionary)) {
      filtered_dictionaries.push_back(dictionary);
    }
  }

  dictionaries_pref.SetValue(filtered_dictionaries);

}

std::vector<std::string> SpellcheckService::GetNormalizedAcceptLanguages(
    bool normalize_for_spellcheck) const {
  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  std::vector<std::string> accept_languages =
      base::SplitString(prefs->GetString(language::prefs::kAcceptLanguages),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (normalize_for_spellcheck) {
    std::ranges::transform(
        accept_languages, accept_languages.begin(),
        [&](const std::string& language) {
          return spellcheck::GetCorrespondingSpellCheckLanguage(language);
        });
  }

  return accept_languages;
}
