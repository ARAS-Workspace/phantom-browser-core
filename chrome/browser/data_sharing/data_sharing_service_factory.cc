// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_service_factory.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_sharing/internal/empty_data_sharing_service.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "content/public/browser/browser_context.h"

namespace data_sharing {
// static
DataSharingServiceFactory* DataSharingServiceFactory::GetInstance() {
  static base::NoDestructor<DataSharingServiceFactory> instance;
  return instance.get();
}

// static
DataSharingService* DataSharingServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<DataSharingService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DataSharingServiceFactory::DataSharingServiceFactory()
    : ProfileKeyedServiceFactory(
          "DataSharingService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

DataSharingServiceFactory::~DataSharingServiceFactory() = default;

std::unique_ptr<KeyedService>
DataSharingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<EmptyDataSharingService>();
}

}  // namespace data_sharing
