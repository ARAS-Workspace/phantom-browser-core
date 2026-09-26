// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/language_model_manager_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/language/content/browser/geo_language_model.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/locale_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/browser/ulp_metrics_logger.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/language/core/common/language_util.h"
#include "components/language/core/language_model/fluent_language_model.h"
#include "components/language/core/language_model/ulp_language_model.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace {

void PrepareLanguageModels(Profile* const profile,
                           language::LanguageModelManager* const manager) {
  // Use the GeoLanguageModel as the primary Language Model if its experiment is
  // enabled, and the FluentLanguageModel otherwise.
  if (language::GetOverrideLanguageModel() ==
      language::OverrideLanguageModel::GEO) {
    manager->AddModel(language::LanguageModelManager::ModelType::GEO,
                      std::make_unique<language::GeoLanguageModel>(
                          language::GeoLanguageProvider::GetInstance()));
    manager->SetPrimaryModel(language::LanguageModelManager::ModelType::GEO);
  } else {
    manager->AddModel(
        language::LanguageModelManager::ModelType::FLUENT,
        std::make_unique<language::FluentLanguageModel>(profile->GetPrefs()));
    manager->SetPrimaryModel(language::LanguageModelManager::ModelType::FLUENT);
  }
}

}  // namespace

// static
LanguageModelManagerFactory* LanguageModelManagerFactory::GetInstance() {
  static base::NoDestructor<LanguageModelManagerFactory> instance;
  return instance.get();
}

// static
language::LanguageModelManager*
LanguageModelManagerFactory::GetForBrowserContext(
    content::BrowserContext* const browser_context) {
  return static_cast<language::LanguageModelManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

LanguageModelManagerFactory::LanguageModelManagerFactory()
    : ProfileKeyedServiceFactory(
          "LanguageModelManager",
          // Use the original profile's language model even in Incognito mode.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

LanguageModelManagerFactory::~LanguageModelManagerFactory() = default;

std::unique_ptr<KeyedService>
LanguageModelManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* const browser_context) const {
  Profile* const profile = Profile::FromBrowserContext(browser_context);
  std::unique_ptr<language::LanguageModelManager> manager =
      std::make_unique<language::LanguageModelManager>(
          profile->GetPrefs(), g_browser_process->GetApplicationLocale());
  PrepareLanguageModels(profile, manager.get());
  return manager;
}
