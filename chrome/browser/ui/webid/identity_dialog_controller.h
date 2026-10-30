// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom-shared.h"
#include "ui/gfx/native_ui_types.h"

namespace content {
class WebContents;

namespace webid {
enum class FederatedLoginResult;
}  // namespace webid
}  // namespace content

using AccountSelectionCallback =
    content::IdentityRequestDialogController::AccountSelectionCallback;
using DismissCallback =
    content::IdentityRequestDialogController::DismissCallback;
using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;
using TokenError = content::IdentityCredentialTokenError;

class IdentityDialogControllerBrowserTest;

// The IdentityDialogController controls the views that are used across
// browser-mediated federated sign-in flows.
class IdentityDialogController
    : public content::IdentityRequestDialogController,
      public AccountSelectionView::Delegate {
 public:
  explicit IdentityDialogController(content::WebContents* rp_web_contents);

  IdentityDialogController(const IdentityDialogController&) = delete;
  IdentityDialogController& operator=(const IdentityDialogController&) = delete;

  ~IdentityDialogController() override;

  // content::IdentityRequestDelegate
  int GetBrandIconMinimumSize(blink::mojom::RpMode rp_mode) override;
  int GetBrandIconIdealSize(blink::mojom::RpMode rp_mode) override;

  // content::IdentityRequestDialogController
  using content::IdentityRequestDialogController::GetPassiveDialogVolume;
  bool ShowAccountsDialog(
      content::RelyingPartyData rp_data,
      const std::vector<IdentityProviderDataPtr>& identity_provider_data,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityRequestAccountPtr>& filtered_accounts,
      blink::mojom::RpMode rp_mode,
      AccountSelectionCallback on_selected,
      LoginToIdPCallback on_add_account,
      DismissCallback dismiss_callback,
      AccountsDisplayedCallback accounts_displayed_callback) override;
  bool ShowFailureDialog(
      const content::RelyingPartyData& rp_data,
      const std::string& idp_for_display,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      const content::IdentityProviderMetadata& idp_metadata,
      const std::vector<IdentityRequestAccountPtr>& filtered_accounts,
      DismissCallback dismiss_callback,
      LoginToIdPCallback login_callback) override;
  bool ShowErrorDialog(const content::RelyingPartyData& rp_data,
                       const std::string& idp_for_display,
                       blink::mojom::RpContext rp_context,
                       blink::mojom::RpMode rp_mode,
                       const content::IdentityProviderMetadata& idp_metadata,
                       const std::optional<TokenError>& error,
                       DismissCallback dismiss_callback,
                       MoreDetailsCallback more_details_callback) override;
  bool ShowLoadingDialog(const content::RelyingPartyData& rp_data,
                         const std::string& idp_for_display,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode,
                         DismissCallback dismiss_callback) override;
  bool ShowVerifyingDialog(
      const content::RelyingPartyData& rp_data,
      const IdentityProviderDataPtr& idp_data,
      const IdentityRequestAccountPtr& account,
      Account::SignInMode sign_in_mode,
      blink::mojom::RpMode rp_mode,
      AccountsDisplayedCallback accounts_displayed_callback) override;

  std::string GetTitle() const override;
  std::optional<std::string> GetSubtitle() const override;

  void ShowUrl(LinkType type, const GURL& url) override;
  // Show a modal dialog that loads content from the IdP in a WebView.
  content::WebContents* ShowModalDialog(
      const GURL& url,
      blink::mojom::RpMode rp_mode,
      DismissCallback dismiss_callback,
      content::IdentityRequestDialogController::ShownModalAsyncCallback
          on_shown_async,
      NativeAppResultCallback native_result_callback) override;
  void CloseModalDialog() override;
  content::WebContents* GetRpWebContents() override;
  void RequestIdPRegistrationPermision(
      const url::Origin& origin,
      base::OnceCallback<void(bool accepted)> callback) override;

  // AccountSelectionView::Delegate:
  void OnAccountSelected(
      const GURL& idp_config_url,
      const std::string& account_id,
      const content::IdentityRequestAccount::LoginState& login_state) override;
  void OnDismiss(DismissReason dismiss_reason) override;
  void OnLoginToIdP(const GURL& idp_config_url,
                    const GURL& idp_login_url) override;
  void OnMoreDetails() override;
  void OnAccountsDisplayed() override;
  void OnNativeAppResult(const std::string& token) override;
  void OnNativeAppLoginFinished() override;
  gfx::NativeView GetNativeView() override;
  content::WebContents* GetWebContents() override;
  content::IdentityRequestDialogController::PassiveDialogVolume
  GetPassiveDialogVolume() const override;

  // Allows setting a mock AccountSelectionView for testing purposes.
  void SetAccountSelectionViewForTesting(
      std::unique_ptr<AccountSelectionView> account_view);

  // Set acting_task_id for testing purposes.

 private:
  friend class IdentityDialogControllerTest;
  friend class IdentityDialogControllerBrowserTest;

  // Attempts to set `account_view_` if it is not already set -- directly on
  // Android, via TabFeatures on desktop.
  bool TrySetAccountView();

  // Whether to show FedCM UI or not.
  bool ShouldShowFedCmUi();

  void DidInvokeShowUi();

  void NotifyEmbedderOfResult(content::webid::FederatedLoginResult result);

  std::unique_ptr<AccountSelectionView> account_view_{nullptr};
  AccountSelectionCallback on_account_selection_;
  DismissCallback on_dismiss_;
  NativeAppResultCallback on_native_result_;
  LoginToIdPCallback on_login_;
  MoreDetailsCallback on_more_details_;
  AccountsDisplayedCallback on_accounts_displayed_;
  raw_ptr<content::WebContents> rp_web_contents_{nullptr};
  blink::mojom::RpMode rp_mode_ = blink::mojom::RpMode::kPassive;
  // Wheter we invoked any show methods. UI may be shown be not invoked in cases
  // such as an active actor task.
  bool did_invoke_show_ui_ = false;
};

#endif  // CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_
