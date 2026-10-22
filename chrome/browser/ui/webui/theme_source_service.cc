// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_source_service.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"

ThemeSourceService::ThemeSourceService(Profile* profile) {
  // The initialization below depends on a typical set of browser threads. Skip
  // it if we are running in a unit test without the full suite.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    return;
  }

  // TODO(crbug.com/40757220): multiple WebUI pages depend on the theme source
  // without adding it themselves. This is not causing an issue because the
  // theme source is being added here. The source should be added where it is
  // used and then the following can be removed.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}

ThemeSourceService::~ThemeSourceService() = default;
