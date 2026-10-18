// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_THEME_FAVICON_SOURCE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_THEME_FAVICON_SOURCE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class ThemeFaviconSourceService;

// Owns all ThemeFaviconSourceServices and associates them with Profiles.
class ThemeFaviconSourceServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the ThemeFaviconSourceService for `profile`.
  static ThemeFaviconSourceService* GetForProfile(Profile* profile);

  static ThemeFaviconSourceServiceFactory* GetInstance();

  ThemeFaviconSourceServiceFactory(const ThemeFaviconSourceServiceFactory&) =
      delete;
  ThemeFaviconSourceServiceFactory& operator=(
      const ThemeFaviconSourceServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ThemeFaviconSourceServiceFactory>;

  ThemeFaviconSourceServiceFactory();
  ~ThemeFaviconSourceServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_THEME_FAVICON_SOURCE_SERVICE_FACTORY_H_
