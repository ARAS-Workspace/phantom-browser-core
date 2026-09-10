// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/upgrade_notification_controller.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/dialogs/outdated_upgrade_bubble.h"

UpgradeNotificationController::~UpgradeNotificationController() = default;

void UpgradeNotificationController::OnOutdatedInstall() {
  Browser* const browser = browser_->GetBrowserForMigrationOnly();
  ShowOutdatedUpgradeBubble(browser, browser, true);
}

void UpgradeNotificationController::OnOutdatedInstallNoAutoUpdate() {
  Browser* const browser = browser_->GetBrowserForMigrationOnly();
  ShowOutdatedUpgradeBubble(browser, browser, false);
}

void UpgradeNotificationController::OnCriticalUpgradeInstalled() {
}

DEFINE_USER_DATA(UpgradeNotificationController);

UpgradeNotificationController::UpgradeNotificationController(
    BrowserWindowInterface* browser)
    : browser_(CHECK_DEREF(browser)),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  upgrade_detector_observation_.Observe(UpgradeDetector::GetInstance());
}

// static
UpgradeNotificationController* UpgradeNotificationController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}
