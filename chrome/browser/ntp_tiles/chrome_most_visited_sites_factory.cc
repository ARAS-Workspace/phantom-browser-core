// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"

#include "build/build_config.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/ntp_tiles/chrome_custom_links_manager_factory.h"
#include "chrome/browser/ntp_tiles/chrome_enterprise_shortcuts_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/history/core/browser/top_sites.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"

namespace {

bool ShouldCreateCustomLinksManager() {
  return true;
}

bool ShouldCreateEnterpriseShortcutsManager() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  return true;
#else
  return false;
#endif
}

}  // namespace

// static
std::unique_ptr<ntp_tiles::MostVisitedSites>
ChromeMostVisitedSitesFactory::NewForProfile(Profile* profile) {
  // MostVisitedSites doesn't exist in incognito profiles.
  if (profile->IsOffTheRecord()) {
    return nullptr;
  }

  auto most_visited_sites = std::make_unique<ntp_tiles::MostVisitedSites>(
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
      TopSitesFactory::GetForProfile(profile), nullptr,
      ShouldCreateCustomLinksManager()
          ? ChromeCustomLinksManagerFactory::NewForProfile(profile)
          : nullptr,
      ShouldCreateEnterpriseShortcutsManager()
          ? ChromeEnterpriseShortcutsManagerFactory::NewForProfile(profile)
          : nullptr,
      nullptr);
  return most_visited_sites;
}
