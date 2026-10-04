// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_CHROME_WEBSTORE_PRIVATE_API_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_CHROME_WEBSTORE_PRIVATE_API_DELEGATE_H_

#include "extensions/browser/api/webstore_private/webstore_private_api_delegate.h"

namespace extensions {

// Chrome implementation of WebstorePrivateAPIDelegate.
class ChromeWebstorePrivateAPIDelegate : public WebstorePrivateAPIDelegate {
 public:
  ChromeWebstorePrivateAPIDelegate();
  ~ChromeWebstorePrivateAPIDelegate() override;

  // WebstorePrivateAPIDelegate:
  std::vector<KeyedServiceBaseFactory*> GetWebStoreAPIFactoryDependencies()
      override;
  signin::IdentityManager* GetIdentityManager(
      content::BrowserContext* context) override;
  void ShowExtensionInstallBlockedDialog(
      content::WebContents* web_contents,
      const Extension* extension,
      const std::u16string& custom_error_message,
      const gfx::ImageSkia& icon,
      base::OnceClosure done_callback) override;
  std::unique_ptr<enterprise_promotion::PromotionEligibilityChecker>
  CreatePromotionEligibilityChecker(content::BrowserContext* context,
                                    bool dismissed_banner_pref,
                                    bool feature_enabled) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_CHROME_WEBSTORE_PRIVATE_API_DELEGATE_H_
