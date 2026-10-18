// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_THEME_FAVICON_SOURCE_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_THEME_FAVICON_SOURCE_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

// Adds the theme (chrome://theme) and the legacy favicon (chrome://favicon)
// data sources of its profile.
class ThemeFaviconSourceService : public KeyedService {
 public:
  explicit ThemeFaviconSourceService(Profile* profile);

  ThemeFaviconSourceService(const ThemeFaviconSourceService&) = delete;
  ThemeFaviconSourceService& operator=(const ThemeFaviconSourceService&) =
      delete;

  ~ThemeFaviconSourceService() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_THEME_FAVICON_SOURCE_SERVICE_H_
