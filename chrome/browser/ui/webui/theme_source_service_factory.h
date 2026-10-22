// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class ThemeSourceService;

// Owns all ThemeSourceServices and associates them with Profiles.
class ThemeSourceServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the ThemeSourceService for `profile`.
  static ThemeSourceService* GetForProfile(Profile* profile);

  static ThemeSourceServiceFactory* GetInstance();

  ThemeSourceServiceFactory(const ThemeSourceServiceFactory&) = delete;
  ThemeSourceServiceFactory& operator=(const ThemeSourceServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<ThemeSourceServiceFactory>;

  ThemeSourceServiceFactory();
  ~ThemeSourceServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_SERVICE_FACTORY_H_
