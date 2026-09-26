// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/chrome_media_router_factory.h"

#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "content/public/browser/browser_context.h"

#include "chrome/browser/media/router/mojo/media_router_desktop.h"

using content::BrowserContext;

namespace media_router {

namespace {

base::LazyInstance<ChromeMediaRouterFactory>::DestructorAtExit service_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ChromeMediaRouterFactory* ChromeMediaRouterFactory::GetInstance() {
  return &service_factory.Get();
}

// static
void ChromeMediaRouterFactory::DoPlatformInit() {}

ChromeMediaRouterFactory::ChromeMediaRouterFactory() = default;

ChromeMediaRouterFactory::~ChromeMediaRouterFactory() = default;

content::BrowserContext* ChromeMediaRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  ProfileSelections profile_selections =
      ProfileSelections::Builder()
          .WithRegular(ProfileSelection::kOwnInstance)
          .WithGuest(ProfileSelection::kOwnInstance)
          .WithSystem(ProfileSelection::kNone)
          // TODO(crbug.com/41488885): Check if this service is needed for
          // Ash Internals.
          .WithAshInternals(ProfileSelection::kOwnInstance)
          .Build();
  return profile_selections.ApplyProfileSelection(
      Profile::FromBrowserContext(context));
}

std::unique_ptr<KeyedService>
ChromeMediaRouterFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
  CHECK(MediaRouterEnabled(context));
  std::unique_ptr<MediaRouterBase> media_router = nullptr;
  media_router = std::make_unique<MediaRouterDesktop>(context);
  media_router->Initialize();
  return media_router;
}

}  // namespace media_router
