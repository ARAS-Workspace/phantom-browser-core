// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_favicon_source_service_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/theme_favicon_source_service.h"

// static
ThemeFaviconSourceService* ThemeFaviconSourceServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ThemeFaviconSourceService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ThemeFaviconSourceServiceFactory*
ThemeFaviconSourceServiceFactory::GetInstance() {
  static base::NoDestructor<ThemeFaviconSourceServiceFactory> instance;
  return instance.get();
}

ThemeFaviconSourceServiceFactory::ThemeFaviconSourceServiceFactory()
    : ProfileKeyedServiceFactory(
          "ThemeFaviconSourceService",
          ProfileSelections::Builder()
              // Regular & Incognito profiles.
              .WithRegular(ProfileSelection::kOwnInstance)
              // Guest profiles.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ThemeServiceFactory::GetInstance());
}

ThemeFaviconSourceServiceFactory::~ThemeFaviconSourceServiceFactory() = default;

std::unique_ptr<KeyedService>
ThemeFaviconSourceServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ThemeFaviconSourceService>(
      Profile::FromBrowserContext(context));
}
