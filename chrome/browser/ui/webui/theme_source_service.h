// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

// Adds the theme (chrome://theme) data source of its profile.
class ThemeSourceService : public KeyedService {
 public:
  explicit ThemeSourceService(Profile* profile);

  ThemeSourceService(const ThemeSourceService&) = delete;
  ThemeSourceService& operator=(const ThemeSourceService&) = delete;

  ~ThemeSourceService() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_SERVICE_H_
