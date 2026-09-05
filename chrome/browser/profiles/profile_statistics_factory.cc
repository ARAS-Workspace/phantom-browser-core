// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/browser_thread.h"

// static
ProfileStatistics* ProfileStatisticsFactory::GetForProfile(Profile* profile) {
  return static_cast<ProfileStatistics*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileStatisticsFactory* ProfileStatisticsFactory::GetInstance() {
  static base::NoDestructor<ProfileStatisticsFactory> instance;
  return instance.get();
}

ProfileStatisticsFactory::ProfileStatisticsFactory()
    : ProfileKeyedServiceFactory(
          "ProfileStatistics",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ProfileStatisticsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ProfileStatistics>(
      BookmarkModelFactory::GetForBrowserContext(profile),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS));
}
