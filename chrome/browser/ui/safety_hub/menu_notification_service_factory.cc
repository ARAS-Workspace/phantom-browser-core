// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"

// static
SafetyHubMenuNotificationServiceFactory*
SafetyHubMenuNotificationServiceFactory::GetInstance() {
  static base::NoDestructor<SafetyHubMenuNotificationServiceFactory> instance;
  return instance.get();
}

// static
SafetyHubMenuNotificationService*
SafetyHubMenuNotificationServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SafetyHubMenuNotificationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SafetyHubMenuNotificationServiceFactory::
    SafetyHubMenuNotificationServiceFactory()
    : ProfileKeyedServiceFactory(
          "SafetyHubMenuNotificationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(RevokedPermissionsServiceFactory::GetInstance());
  DependsOn(NotificationPermissionsReviewServiceFactory::GetInstance());
}

SafetyHubMenuNotificationServiceFactory::
    ~SafetyHubMenuNotificationServiceFactory() = default;

std::unique_ptr<KeyedService>
SafetyHubMenuNotificationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  RevokedPermissionsService* revoked_permissions_service =
      RevokedPermissionsServiceFactory::GetForProfile(profile);
  NotificationPermissionsReviewService* notification_permission_review_service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile);
  return std::make_unique<SafetyHubMenuNotificationService>(
      profile->GetPrefs(), revoked_permissions_service,
      notification_permission_review_service, profile);
}
