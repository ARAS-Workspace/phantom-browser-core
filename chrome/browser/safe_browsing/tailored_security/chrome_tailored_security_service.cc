// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_notification_result.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_education/product_messaging/product_messaging_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

#include "chrome/browser/safe_browsing/tailored_security/notification_handler_desktop.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/safe_browsing/tailored_security_desktop_dialog_manager.h"
#include "chrome/browser/user_education/user_education_service_factory.h"

DEFINE_PRODUCT_MESSAGE_KEY(kEnabledEnhancedBrowsingNotice);
DEFINE_PRODUCT_MESSAGE_KEY(kDisabledEnhancedBrowsingNotice);

namespace safe_browsing {

namespace {}  // namespace

ChromeTailoredSecurityService::ChromeTailoredSecurityService(Profile* profile)
    : TailoredSecurityService(IdentityManagerFactory::GetForProfile(profile),
                              SyncServiceFactory::GetForProfile(profile),
                              profile->GetPrefs()),
      profile_(profile),
      retry_handler_(std::make_unique<MessageRetryHandler>(
          profile_,
          prefs::kTailoredSecuritySyncFlowRetryState,
          prefs::kTailoredSecurityNextSyncFlowTimestamp,
          kRetryAttemptStartupDelay,
          kRetryNextAttemptDelay,
          kWaitingPeriodInterval,
          base::BindOnce(&ChromeTailoredSecurityService::
                             TailoredSecurityTimestampUpdateCallback,
                         base::Unretained(this)),
          "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
          prefs::kAccountTailoredSecurityUpdateTimestamp,
          prefs::kEnhancedProtectionEnabledViaTailoredSecurity)) {
  AddObserver(this);
  if (HistorySyncEnabledForUser() &&
      !SafeBrowsingPolicyHandler::IsSafeBrowsingProtectionLevelSetByPolicy(
          prefs())) {
    retry_handler_->StartRetryTimer();
  }
}

ChromeTailoredSecurityService::~ChromeTailoredSecurityService() {
  RemoveObserver(this);
}

void ChromeTailoredSecurityService::OnSyncNotificationMessageRequest(
    bool is_enabled) {
  ProfileBrowserCollection* const collection =
      ProfileBrowserCollection::GetForProfile(profile_);
  BrowserWindowInterface* browser =
      collection ? collection->GetLastActiveBrowser() : nullptr;
  if (!browser) {
    if (is_enabled) {
      RecordEnabledNotificationResult(
          TailoredSecurityNotificationResult::kNoBrowserAvailable);
    }
    return;
  }
  if (!browser->GetWindow()) {
    if (is_enabled) {
      RecordEnabledNotificationResult(
          TailoredSecurityNotificationResult::kNoBrowserWindowAvailable);
    }
    return;
  }
  TailoredSecurityService::ScopedSyncNotificationGuard guard(*this);

  // TODO(crbug.com/483786422): Register preference change handlers in each
  // relevant generated.*pref class that acts whenever the settings bundle
  // setting changes.
  if (base::FeatureList::IsEnabled(safe_browsing::kBundledSecuritySettings)) {
    PrefService* profile_pref = profile_->GetPrefs();
    bool tailored_security_pref_registered = profile_pref->FindPreference(
        prefs::kEnhancedProtectionEnabledViaTailoredSecurity);
    if (tailored_security_pref_registered) {
      SetSecurityBundleSetting(
          *profile_pref, is_enabled ? SecuritySettingsBundleSetting::ENHANCED
                                    : SecuritySettingsBundleSetting::STANDARD);
      profile_pref->SetBoolean(
          prefs::kEnhancedProtectionEnabledViaTailoredSecurity, is_enabled);
    }
  } else {
    SetSafeBrowsingState(profile_->GetPrefs(),
                         is_enabled ? SafeBrowsingState::ENHANCED_PROTECTION
                                    : SafeBrowsingState::STANDARD_PROTECTION,
                         /*is_esb_enabled_by_account_integration=*/is_enabled);
  }

  if (base::FeatureList::IsEnabled(safe_browsing::kNoticeQueueForEsb)) {
    QueueNotice(is_enabled);
  } else {
    DisplayDesktopDialog(browser, is_enabled);
  }

  retry_handler_->SaveRetryState(
      MessageRetryHandler::RetryState::NO_RETRY_NEEDED);

  if (is_enabled) {
    RecordEnabledNotificationResult(TailoredSecurityNotificationResult::kShown);
  }
}

void ChromeTailoredSecurityService::TriggerDialogDisplay(
    bool is_enabled,
    user_education::ProductMessagingHandle messaging_priority_handle) {
  if (is_enabled) {
    enabled_notice_handle_ = std::move(messaging_priority_handle);
  } else {
    disabled_notice_handle_ = std::move(messaging_priority_handle);
  }
  ProfileBrowserCollection* const collection =
      ProfileBrowserCollection::GetForProfile(profile_);
  BrowserWindowInterface* browser =
      collection ? collection->GetLastActiveBrowser() : nullptr;
  DisplayDesktopDialog(browser, is_enabled);
}

void ChromeTailoredSecurityService::ReleaseEnabledQueueHandle() {
  enabled_notice_handle_.reset();
}

void ChromeTailoredSecurityService::ReleaseDisabledQueueHandle() {
  disabled_notice_handle_.reset();
}

void ChromeTailoredSecurityService::QueueNotice(bool is_enabled) {
  // When we want to display the dialog, add it to the queue. More likely than
  // not, there will not be other items in the queue, so it was display
  // immediately. In edge cases, it will display after other entries in the
  // queue have processed.
  auto& product_messaging_controller =
      UserEducationServiceFactory::GetForBrowserContext(profile_)
          ->product_messaging_controller();

  const auto& notice_to_queue = is_enabled ? kEnabledEnhancedBrowsingNotice
                                           : kDisabledEnhancedBrowsingNotice;

  // We reference the handle of the opposite state (e.g., if enabling, we look
  // at the disabled handle).
  auto& other_notice_handle =
      is_enabled ? disabled_notice_handle_ : enabled_notice_handle_;

  if (product_messaging_controller.GetMessageStatus(notice_to_queue) ==
      user_education::ProductMessageStatus::kNone) {
    // If the conflicting notice is currently held, release it so the new one
    // can process.
    if (other_notice_handle) {
      other_notice_handle.reset();
    }

    product_messaging_controller.QueueMessage(
        notice_to_queue,
        base::BindOnce(&ChromeTailoredSecurityService::TriggerDialogDisplay,
                       weak_factory_.GetWeakPtr(), is_enabled));
  }
}

void ChromeTailoredSecurityService::DisplayDesktopDialog(
    BrowserWindowInterface* browser,
    bool show_enable_modal) {
  if (show_enable_modal) {
    dialog_manager_.ShowEnabledDialogForBrowser(
        browser, base::BindOnce(
                     &ChromeTailoredSecurityService::ReleaseEnabledQueueHandle,
                     weak_factory_.GetWeakPtr()));
  } else {
    dialog_manager_.ShowDisabledDialogForBrowser(
        browser, base::BindOnce(
                     &ChromeTailoredSecurityService::ReleaseDisabledQueueHandle,
                     weak_factory_.GetWeakPtr()));
  }
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeTailoredSecurityService::GetURLLoaderFactory() {
  return profile_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

}  // namespace safe_browsing
