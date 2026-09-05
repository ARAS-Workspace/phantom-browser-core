// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_

#include <memory>
#include <string_view>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safety_hub_test_util {

// Creates a new instance of the RevokedPermissionsService and associates it
// with |context|.
void CreateRevokedPermissionsService(content::BrowserContext* context);

// Creates a new instance of the NotificationPermissionsReviewService and
// associates it with |context|.
void CreateNotificationPermissionsReviewService(
    content::BrowserContext* context);

// This will run the UpdateAsync function on the provided SafetyHubService and
// return when both the background task and UI task are completed. It will
// temporary add an observer to the service, which will be removed again before
// the function returns.
void UpdateSafetyHubServiceAsync(SafetyHubService* service);

// This will run the UpdateAsync function on the RevokedPermissionsService
// and return when both the background task and UI task are completed. This
// separate helper is needed because abusive notification revocation is
// asynchronous, so this method should run until idle.
void UpdateRevokedPermissionsServiceAsync(RevokedPermissionsService* service);

// Returns true if the provided list of content settings has a setting with the
// provided url.
bool IsUrlInSettingsList(ContentSettingsForOneType content_settings, GURL url);

// Create a series of notifications on a site that has not been interacted with
// that will result in a Safety Hub menu notification being shown.
void GenerateSafetyHubMenuNotification(Profile* profile);

}  // namespace safety_hub_test_util

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_
