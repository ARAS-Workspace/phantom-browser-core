// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_private/chrome_webstore_private_api_delegate.h"

#include <memory>

#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/extensions/extension_allowlist_factory.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/browser/extension_allowlist.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/browser_process.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#endif

namespace extensions {

ChromeWebstorePrivateAPIDelegate::ChromeWebstorePrivateAPIDelegate() = default;

ChromeWebstorePrivateAPIDelegate::~ChromeWebstorePrivateAPIDelegate() = default;

std::vector<KeyedServiceBaseFactory*>
ChromeWebstorePrivateAPIDelegate::GetWebStoreAPIFactoryDependencies() {
  std::vector<KeyedServiceBaseFactory*> dependencies;
  dependencies.push_back(ExtensionAllowlistFactory::GetInstance());
  dependencies.push_back(IdentityManagerFactory::GetInstance());
  dependencies.push_back(InstallTrackerFactory::GetInstance());
  return dependencies;
}

ExtensionAllowlist* ChromeWebstorePrivateAPIDelegate::GetExtensionAllowlist(
    content::BrowserContext* context) {
  return ExtensionAllowlistFactory::GetForBrowserContext(context);
}

signin::IdentityManager* ChromeWebstorePrivateAPIDelegate::GetIdentityManager(
    content::BrowserContext* context) {
  return IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context));
}

void ChromeWebstorePrivateAPIDelegate::ShowExtensionInstallBlockedDialog(
    content::WebContents* web_contents,
    const Extension* extension,
    const std::u16string& custom_error_message,
    const gfx::ImageSkia& icon,
    base::OnceClosure done_callback) {
  ::extensions::ShowExtensionInstallBlockedDialog(
      extension->id(), extension->name(), custom_error_message, icon,
      web_contents, std::move(done_callback));
}

void ChromeWebstorePrivateAPIDelegate::ShowExtensionInstallFrictionDialog(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback) {
  ::extensions::ShowExtensionInstallFrictionDialog(web_contents,
                                                   std::move(callback));
}

void ChromeWebstorePrivateAPIDelegate::ReportFrictionAcceptedEvent(
    content::BrowserContext* context) {
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
bool ChromeWebstorePrivateAPIDelegate::IsSafeBrowsingEnabledAndReady(
    content::BrowserContext* context) {
  return false;
}

safe_browsing::SafeBrowsingNavigationObserverManager*
ChromeWebstorePrivateAPIDelegate::GetSafeBrowsingNavigationObserverManager(
    content::BrowserContext* context) {
  return nullptr;
}
#endif

std::unique_ptr<enterprise_promotion::PromotionEligibilityChecker>
ChromeWebstorePrivateAPIDelegate::CreatePromotionEligibilityChecker(
    content::BrowserContext* context,
    bool dismissed_banner_pref,
    bool feature_enabled) {
  return nullptr;
}

}  // namespace extensions
