// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_service_factory.h"

#include <memory>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/skills/public/skills_features.h"
#include "components/skills/public/skills_prefs.h"

namespace skills {

SkillsService* SkillsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SkillsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
bool SkillsServiceFactory::IsSkillsEnabledForProfile(Profile* profile) {
  return profile && skills::IsSkillsEnabled(profile->GetPrefs());
}

SkillsServiceFactory* SkillsServiceFactory::GetInstance() {
  static base::NoDestructor<SkillsServiceFactory> instance;
  return instance.get();
}

SkillsServiceFactory::SkillsServiceFactory()
    : ProfileKeyedServiceFactory(
          "SkillsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

SkillsServiceFactory::~SkillsServiceFactory() = default;

std::unique_ptr<KeyedService>
SkillsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return nullptr;
}

}  // namespace skills
