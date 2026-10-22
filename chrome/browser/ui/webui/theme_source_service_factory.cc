// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_source_service_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/theme_source_service.h"

// static
ThemeSourceService* ThemeSourceServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ThemeSourceService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ThemeSourceServiceFactory* ThemeSourceServiceFactory::GetInstance() {
  static base::NoDestructor<ThemeSourceServiceFactory> instance;
  return instance.get();
}

ThemeSourceServiceFactory::ThemeSourceServiceFactory()
    : ProfileKeyedServiceFactory(
          "ThemeSourceService",
          ProfileSelections::Builder()
              // Regular & Incognito profiles.
              .WithRegular(ProfileSelection::kOwnInstance)
              // Guest profiles.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ThemeServiceFactory::GetInstance());
}

ThemeSourceServiceFactory::~ThemeSourceServiceFactory() = default;

std::unique_ptr<KeyedService>
ThemeSourceServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ThemeSourceService>(
      Profile::FromBrowserContext(context));
}
