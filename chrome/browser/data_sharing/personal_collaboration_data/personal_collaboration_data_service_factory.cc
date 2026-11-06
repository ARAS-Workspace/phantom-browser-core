// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/personal_collaboration_data/personal_collaboration_data_service_factory.h"

#include <memory>

#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#include "content/public/browser/browser_context.h"

namespace data_sharing::personal_collaboration_data {

// static
PersonalCollaborationDataServiceFactory*
PersonalCollaborationDataServiceFactory::GetInstance() {
  static base::NoDestructor<PersonalCollaborationDataServiceFactory> instance;
  return instance.get();
}

// static
PersonalCollaborationDataService*
PersonalCollaborationDataServiceFactory::GetForProfile(Profile* profile) {
  CHECK(profile);
  return static_cast<PersonalCollaborationDataService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

PersonalCollaborationDataServiceFactory::
    PersonalCollaborationDataServiceFactory()
    : ProfileKeyedServiceFactory(
          "PersonalCollaborationDataService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

PersonalCollaborationDataServiceFactory::
    ~PersonalCollaborationDataServiceFactory() = default;

std::unique_ptr<KeyedService>
PersonalCollaborationDataServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(!context->IsOffTheRecord());
  return nullptr;
}

}  // namespace data_sharing::personal_collaboration_data
