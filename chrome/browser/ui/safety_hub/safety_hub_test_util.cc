// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/password_manager/factories/bulk_leak_check_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service_factory.h"
#include "components/content_settings/core/common/content_settings.h"
#include "services/network/test/test_shared_url_loader_factory.h"

#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
namespace {

std::unique_ptr<KeyedService> BuildRevokedPermissionsService(
    content::BrowserContext* context) {
  return std::make_unique<RevokedPermissionsService>(
      context, Profile::FromBrowserContext(context)->GetPrefs());
}

std::unique_ptr<KeyedService> BuildNotificationPermissionsReviewService(
    content::BrowserContext* context) {
  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(
          Profile::FromBrowserContext(context));
  return std::make_unique<NotificationPermissionsReviewService>(
      HostContentSettingsMapFactory::GetForProfile(context),
      engagement_service);
}
class TestObserver : public SafetyHubService::Observer {
 public:
  void SetCallback(const base::RepeatingClosure& callback) {
    callback_ = callback;
  }

  void OnResultAvailable(const SafetyHubResult* result) override {
    callback_.Run();
  }

 private:
  base::RepeatingClosure callback_;
};

}  // namespace

namespace safety_hub_test_util {

void CreateRevokedPermissionsService(content::BrowserContext* context) {
  RevokedPermissionsServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildRevokedPermissionsService));
}

void CreateNotificationPermissionsReviewService(
    content::BrowserContext* context) {
  NotificationPermissionsReviewServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildNotificationPermissionsReviewService));
}

void UpdateSafetyHubServiceAsync(SafetyHubService* service) {
  auto test_observer = std::make_shared<TestObserver>();
  service->AddObserver(test_observer.get());
  // We need to check if there is any update process currently active, and wait
  // until all have completed before running another update.
  while (service->IsUpdateRunning()) {
    base::RunLoop ongoing_update_loop;
    test_observer->SetCallback(ongoing_update_loop.QuitClosure());
    ongoing_update_loop.Run();
  }
  base::RunLoop loop;
  test_observer->SetCallback(loop.QuitClosure());
  service->UpdateAsync();
  loop.Run();
  service->RemoveObserver(test_observer.get());
}

void UpdateRevokedPermissionsServiceAsync(RevokedPermissionsService* service) {
  // Run until the checks complete for unused site permission revocation.
  UpdateSafetyHubServiceAsync(service);

  // Run until the checks complete for abusive notification revocation.
  base::RunLoop().RunUntilIdle();
}

bool IsUrlInSettingsList(ContentSettingsForOneType content_settings, GURL url) {
  for (const auto& setting : content_settings) {
    if (setting.primary_pattern.ToRepresentativeUrl() == url) {
      return true;
    }
  }
  return false;
}

void GenerateSafetyHubMenuNotification(Profile* profile) {
  // Creating and showing a notification for a site that has never been
  // interacted with, will be caught by the notification permission review
  // service, and raised as a Safety Hub issue to be reviewed. In this case a
  // menu entry should be there with the action to open the Safety Hub
  // settings page.
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile);
  const GURL kUrl("https://example.com");
  hcsm->SetContentSettingDefaultScope(
      kUrl, GURL(), content_settings::mojom::ContentSettingsType::NOTIFICATIONS,
      CONTENT_SETTING_ALLOW);
  auto* notifications_engagement_service =
      NotificationsEngagementServiceFactory::GetForProfile(profile);
  // There should be at least an average of 1 recorded notification per day,
  // for the past week to trigger a Safety Hub review.
  notifications_engagement_service->RecordNotificationDisplayed(kUrl, 7);

  // Update the notification permissions review service for it to capture the
  // recently added notification permission.
  auto* notification_permissions_service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile);
  safety_hub_test_util::UpdateSafetyHubServiceAsync(
      notification_permissions_service);
}

}  // namespace safety_hub_test_util
