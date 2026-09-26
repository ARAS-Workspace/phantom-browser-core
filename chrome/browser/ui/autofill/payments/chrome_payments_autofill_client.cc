// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_offer_manager_factory.h"
#include "chrome/browser/autofill/merchant_promo_code_manager_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/autofill/payments/credit_card_scanner_controller.h"
#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/risk_util.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_payment_method_delegate.h"
#include "components/autofill/core/browser/metrics/payments/risk_data_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#include "components/autofill/core/browser/payments/otp_unmask_result.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_churned_users_manager.h"
#include "components/autofill/core/browser/payments/payments_churned_users_metrics.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager_impl.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_ui_type.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller_impl.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#include "chrome/browser/ui/autofill/payments/desktop_bnpl_ui_delegate.h"
#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager.h"
#include "chrome/browser/ui/autofill/payments/filled_card_information_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_page_action_controller.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_state.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"  // nogncheck
#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promos_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/autofill/core/browser/payments/desktop_bnpl_strategy.h"
#include "components/autofill/core/browser/ui/payments/omnibox_autofill_delegate.h"
#include "components/autofill/core/common/autofill_payments_features.h"
// TODO(crbug.com/407105162): Remove nogncheck when crbug.com/40147906 is fixed.
#include "components/tabs/public/tab_interface.h"  // nogncheck
#include "components/webauthn/content/browser/internal_authenticator_impl.h"

// TODO(crbug.com/407106692): Refactor for Platform-Specific Code Separation.
namespace autofill::payments {

ChromePaymentsAutofillClient::ChromePaymentsAutofillClient(
    ContentAutofillClient* client)
    : content::WebContentsObserver(&client->GetWebContents()),
      client_(CHECK_DEREF(client)),
      save_and_fill_manager_(
          std::make_unique<SaveAndFillManagerImpl>(&client_.get())),
      payments_churned_users_manager_(
          std::make_unique<payments::PaymentsChurnedUsersManager>(client)) {
  if (base::FeatureList::IsEnabled(features::kAutofillEnableOmniboxAutofill)) {
    omnibox_autofill_delegate_ =
        std::make_unique<OmniboxAutofillDelegate>(&client_.get());
  }
}

ChromePaymentsAutofillClient::~ChromePaymentsAutofillClient() = default;

void ChromePaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  if (!risk_data_.empty() &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnablePrefetchingRiskDataForRetrieval)) {
    // Notify tests that the cached risk data was used and new risk data was not
    // loaded, if the callback exists.
    if (cached_risk_data_loaded_callback_for_testing_) {
      std::move(cached_risk_data_loaded_callback_for_testing_).Run(risk_data_);
      return;
    }
    std::move(callback).Run(risk_data_);
    return;
  }
  risk_util::LoadRiskData(
      0, web_contents(),
      base::BindOnce(&ChromePaymentsAutofillClient::OnRiskDataLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     base::TimeTicks::Now()));
}

void ChromePaymentsAutofillClient::ShowWebauthnOfferDialog(
    WebauthnDialogCallback offer_dialog_callback) {
  WebauthnDialogControllerImpl::GetOrCreateForPage(
      web_contents()->GetPrimaryPage())
      ->ShowOfferDialog(std::move(offer_dialog_callback));
}

void ChromePaymentsAutofillClient::ShowWebauthnVerifyPendingDialog(
    WebauthnDialogCallback verify_pending_dialog_callback) {
  WebauthnDialogControllerImpl::GetOrCreateForPage(
      web_contents()->GetPrimaryPage())
      ->ShowVerifyPendingDialog(std::move(verify_pending_dialog_callback));
}

void ChromePaymentsAutofillClient::UpdateWebauthnOfferDialogWithError() {
  WebauthnDialogControllerImpl* controller =
      WebauthnDialogControllerImpl::GetForPage(
          web_contents()->GetPrimaryPage());
  if (controller) {
    controller->UpdateDialog(WebauthnDialogState::kOfferError);
  }
}

bool ChromePaymentsAutofillClient::CloseWebauthnDialog() {
  WebauthnDialogControllerImpl* controller =
      WebauthnDialogControllerImpl::GetForPage(
          web_contents()->GetPrimaryPage());
  if (controller) {
    return controller->CloseDialog();
  }

  return false;
}

void ChromePaymentsAutofillClient::
    HideVirtualCardEnrollBubbleAndIconIfVisible() {
  VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents());
  VirtualCardEnrollBubbleControllerImpl* controller =
      VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents());

  if (controller && controller->IsIconVisible()) {
    controller->HideIconAndBubble();
  }
}

bool ChromePaymentsAutofillClient::HasCreditCardScanFeature() const {
  return CreditCardScannerController::HasCreditCardScanFeature();
}

void ChromePaymentsAutofillClient::ScanCreditCard(
    ChromePaymentsAutofillClient::CreditCardScanCallback callback) {
  CreditCardScannerController::ScanCreditCard(web_contents(),
                                              std::move(callback));
}

bool ChromePaymentsAutofillClient::LocalCardSaveIsSupported() {
  return true;
}

void ChromePaymentsAutofillClient::ShowSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  SaveCardBubbleControllerImpl::FromWebContents(web_contents())
      ->OfferLocalSave(card, options, std::move(callback));
}

void ChromePaymentsAutofillClient::ShowSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  // Hide virtual card confirmation bubble showing for a different card.
  HideVirtualCardEnrollBubbleAndIconIfVisible();

  // Do lazy initialization of SaveCardBubbleControllerImpl.
  SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  SaveCardBubbleControllerImpl::FromWebContents(web_contents())
      ->OfferUploadSave(card, legal_message_lines, options,
                        std::move(callback));
}

void ChromePaymentsAutofillClient::CreditCardUploadCompleted(
    PaymentsRpcResult result,
    std::optional<OnConfirmationClosedCallback>
        on_confirmation_closed_callback) {
  const bool card_saved = result == PaymentsRpcResult::kSuccess;
  if (result == PaymentsRpcResult::kClientSideTimeout) {
    HideSaveCardPrompt();
    return;
  }
  // This feedback is also used by Save and Fill flow where
  // SaveCardBubbleControllerImpl is not created beforehand.
  if (SaveCardBubbleControllerImpl* controller =
          SaveCardBubbleControllerImpl::GetOrCreateForWebContents(
              web_contents())) {
    // Only attempt to show the iOS payment promo if the card was successfully
    // uploaded and there is no VCN enroll flow callback, and still fallback to
    // normal confirmation bubble if showing the promo fails.
    if (card_saved && !on_confirmation_closed_callback) {
      base::OnceClosure promo_will_show_callback =
          controller->GetEndSaveCardPromptFlowCallback();
      base::OnceClosure promo_not_shown_callback =
          controller->GetShowConfirmationForCardSuccessfullySavedCallback();

      BrowserWindowInterface* browser =
          GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
              web_contents());

      if (!browser) {
        std::move(promo_not_shown_callback).Run();
        return;
      }

      ios_promos_utils::MaybeOverrideCardConfirmationBubbleWithIOSPaymentPromo(
          browser->GetBrowserForMigrationOnly(),
          std::move(promo_will_show_callback),
          std::move(promo_not_shown_callback));

      return;
    }

    controller->ShowConfirmationBubbleView(
        card_saved, /*is_for_save_and_fill=*/false,
        std::move(on_confirmation_closed_callback));
  }
}

void ChromePaymentsAutofillClient::HideSaveCardPrompt() {
  SaveCardBubbleControllerImpl* controller =
      SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  if (controller) {
    controller->HideSaveCardBubble();
  }
}

void ChromePaymentsAutofillClient::ShowVirtualCardEnrollDialog(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {
  VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents());
  VirtualCardEnrollBubbleControllerImpl* controller =
      VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents());
  DCHECK(controller);
  controller->SetupAndShowBubble(virtual_card_enrollment_fields,
                                 std::move(accept_virtual_card_callback),
                                 std::move(decline_virtual_card_callback));
}

void ChromePaymentsAutofillClient::VirtualCardEnrollCompleted(
    PaymentsRpcResult result) {
  VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents());
  VirtualCardEnrollBubbleControllerImpl* controller =
      VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents());

  if (controller) {
    // Called by clank to close AutofillVCNEnrollBottomSheetBridge.
    // TODO(crbug.com/350713949): Extract AutofillVCNEnrollBottomSheetBridge
    // so the controller only needs to be called for desktop.
    controller->ShowConfirmationBubbleView(result);
  }
}

void ChromePaymentsAutofillClient::OnCardDataAvailable(
    const FilledCardInformationBubbleOptions& options,
    const url::Origin& origin) {
  FilledCardInformationBubbleControllerImpl::CreateForWebContents(
      web_contents());
  FilledCardInformationBubbleControllerImpl* controller =
      FilledCardInformationBubbleControllerImpl::FromWebContents(
          web_contents());
  controller->SetupAndShowBubble(options);
}

void ChromePaymentsAutofillClient::ConfirmSaveIbanLocally(
    const Iban& iban,
    bool should_show_prompt,
    SaveIbanPromptCallback callback) {
  // Do lazy initialization of IbanBubbleControllerImpl.
  IbanBubbleControllerImpl::CreateForWebContents(web_contents());
  IbanBubbleControllerImpl::FromWebContents(web_contents())
      ->OfferLocalSave(iban, should_show_prompt, std::move(callback));
}

void ChromePaymentsAutofillClient::ConfirmUploadIbanToCloud(
    const Iban& iban,
    LegalMessageLines legal_message_lines,
    bool should_show_prompt,
    SaveIbanPromptCallback callback) {
  // Do lazy initialization of IbanBubbleControllerImpl.
  IbanBubbleControllerImpl::CreateForWebContents(web_contents());
  IbanBubbleControllerImpl::FromWebContents(web_contents())
      ->OfferUploadSave(iban, std::move(legal_message_lines),
                        should_show_prompt, std::move(callback));
}

void ChromePaymentsAutofillClient::IbanUploadCompleted(bool iban_saved,
                                                       bool hit_max_strikes) {
  if (IbanBubbleControllerImpl* controller =
          IbanBubbleControllerImpl::FromWebContents(web_contents())) {
    controller->ShowConfirmationBubbleView(iban_saved, hit_max_strikes);
  }
}

void ChromePaymentsAutofillClient::ShowAutofillProgressDialog(
    AutofillProgressUiType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  autofill_progress_dialog_controller_ =
      std::make_unique<AutofillProgressDialogControllerImpl>(
          autofill_progress_dialog_type, std::move(cancel_callback));
  autofill_progress_dialog_controller_->ShowDialog(
      base::BindOnce(&CreateAndShowProgressDialog,
                     autofill_progress_dialog_controller_->GetWeakPtr(),
                     base::Unretained(web_contents())));
}

void ChromePaymentsAutofillClient::CloseAutofillProgressDialog(
    bool show_confirmation_before_closing,
    base::OnceClosure no_interactive_authentication_callback) {
  DCHECK(autofill_progress_dialog_controller_);
  autofill_progress_dialog_controller_->DismissDialog(
      show_confirmation_before_closing,
      std::move(no_interactive_authentication_callback));
}

void ChromePaymentsAutofillClient::ShowCardUnmaskOtpInputDialog(
    CreditCard::RecordType card_type,
    const CardUnmaskChallengeOption& challenge_option,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  card_unmask_otp_input_dialog_controller_ =
      std::make_unique<CardUnmaskOtpInputDialogControllerImpl>(
          card_type, challenge_option, delegate);
  card_unmask_otp_input_dialog_controller_->ShowDialog(
      base::BindOnce(&CreateAndShowOtpInputDialog,
                     card_unmask_otp_input_dialog_controller_->GetWeakPtr(),
                     base::Unretained(web_contents())));
}

void ChromePaymentsAutofillClient::OnUnmaskOtpVerificationResult(
    OtpUnmaskResult unmask_result) {
  if (card_unmask_otp_input_dialog_controller_) {
    card_unmask_otp_input_dialog_controller_->OnOtpVerificationResult(
        unmask_result);
  }
}

void ChromePaymentsAutofillClient::ShowUnmaskAuthenticatorSelectionDialog(
    const std::vector<CardUnmaskChallengeOption>& challenge_options,
    base::OnceCallback<void(const std::string&)>
        confirm_unmask_challenge_option_callback,
    base::OnceClosure cancel_unmasking_closure) {
  card_unmask_authentication_selection_controller_ =
      std::make_unique<CardUnmaskAuthenticationSelectionDialogControllerImpl>(
          challenge_options,
          std::move(confirm_unmask_challenge_option_callback),
          std::move(cancel_unmasking_closure));
  card_unmask_authentication_selection_controller_->ShowDialog(
      base::BindOnce(&CreateAndShowCardUnmaskAuthenticationSelectionDialog,
                     base::Unretained(web_contents())));
}

void ChromePaymentsAutofillClient::DismissUnmaskAuthenticatorSelectionDialog(
    bool server_success) {
  if (card_unmask_authentication_selection_controller_) {
    card_unmask_authentication_selection_controller_
        ->DismissDialogUponServerProcessedAuthenticationMethodRequest(
            server_success);
    card_unmask_authentication_selection_controller_.reset();
  }
}

PaymentsNetworkInterface*
ChromePaymentsAutofillClient::GetPaymentsNetworkInterface() {
  if (!payments_network_interface_) {
    payments_network_interface_ = std::make_unique<PaymentsNetworkInterface>(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->GetURLLoaderFactory(),
        client_->GetIdentityManager(),
        &client_->GetPersonalDataManager().payments_data_manager(),
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->IsOffTheRecord());
  }
  return payments_network_interface_.get();
}

MultipleRequestPaymentsNetworkInterface*
ChromePaymentsAutofillClient::GetMultipleRequestPaymentsNetworkInterface() {
  if (!multiple_request_payments_network_interface_) {
    multiple_request_payments_network_interface_ =
        std::make_unique<MultipleRequestPaymentsNetworkInterface>(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext())
                ->GetURLLoaderFactory(),
            *client_->GetIdentityManager(),
            Profile::FromBrowserContext(web_contents()->GetBrowserContext())
                ->IsOffTheRecord());
  }
  return multiple_request_payments_network_interface_.get();
}

void ChromePaymentsAutofillClient::ShowAutofillErrorDialog(
    AutofillErrorDialogContext context) {
  autofill_error_dialog_controller_ =
      std::make_unique<AutofillErrorDialogControllerImpl>(std::move(context));
  autofill_error_dialog_controller_->Show(
      base::BindOnce(&CreateAndShowAutofillErrorDialog,
                     base::Unretained(autofill_error_dialog_controller_.get()),
                     base::Unretained(web_contents())));
}

PaymentsWindowManager*
ChromePaymentsAutofillClient::GetPaymentsWindowManager() {
  if (!payments_window_manager_) {
    payments_window_manager_ =
        std::make_unique<DesktopPaymentsWindowManager>(&client_.get());
  }

  return payments_window_manager_.get();
}

void ChromePaymentsAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_ = std::make_unique<CardUnmaskPromptControllerImpl>(
      user_prefs::UserPrefs::Get(client_->GetWebContents().GetBrowserContext()),
      card, card_unmask_prompt_options, delegate);
  unmask_controller_->ShowPrompt(base::BindOnce(
      &CreateCardUnmaskPromptView, base::Unretained(unmask_controller_.get()),
      base::Unretained(web_contents())));
}


// TODO(crbug.com/40186650): Refactor this for both CVC and Biometrics flows.
void ChromePaymentsAutofillClient::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  if (unmask_controller_) {
    unmask_controller_->OnVerificationResult(result);
  }
}

VirtualCardEnrollmentManager*
ChromePaymentsAutofillClient::GetVirtualCardEnrollmentManager() {
  if (!virtual_card_enrollment_manager_) {
    virtual_card_enrollment_manager_ =
        std::make_unique<VirtualCardEnrollmentManager>(
            &client_->GetPersonalDataManager().payments_data_manager(),
            GetMultipleRequestPaymentsNetworkInterface(), &client_.get());
  }

  return virtual_card_enrollment_manager_.get();
}

CreditCardCvcAuthenticator&
ChromePaymentsAutofillClient::GetCvcAuthenticator() {
  if (!cvc_authenticator_) {
    cvc_authenticator_ =
        std::make_unique<CreditCardCvcAuthenticator>(&client_.get());
  }
  return *cvc_authenticator_;
}

CreditCardOtpAuthenticator*
ChromePaymentsAutofillClient::GetOtpAuthenticator() {
  if (!otp_authenticator_) {
    otp_authenticator_ =
        std::make_unique<CreditCardOtpAuthenticator>(&client_.get());
  }
  return otp_authenticator_.get();
}

CreditCardRiskBasedAuthenticator*
ChromePaymentsAutofillClient::GetRiskBasedAuthenticator() {
  if (!risk_based_authenticator_) {
    risk_based_authenticator_ =
        std::make_unique<CreditCardRiskBasedAuthenticator>(&client_.get());
  }
  return risk_based_authenticator_.get();
}

bool ChromePaymentsAutofillClient::IsMandatoryReauthEnabled() {
  return GetPaymentsDataManager().IsPaymentMethodsMandatoryReauthEnabled();
}

void ChromePaymentsAutofillClient::ShowMandatoryReauthOptInPrompt(
    base::OnceClosure accept_mandatory_reauth_callback,
    base::OnceClosure cancel_mandatory_reauth_callback,
    base::RepeatingClosure close_mandatory_reauth_callback) {
  MandatoryReauthBubbleControllerImpl::CreateForWebContents(web_contents());
  MandatoryReauthBubbleControllerImpl::FromWebContents(web_contents())
      ->SetupAndShowBubble(std::move(accept_mandatory_reauth_callback),
                           std::move(cancel_mandatory_reauth_callback),
                           std::move(close_mandatory_reauth_callback));
}

void ChromePaymentsAutofillClient::ShowMandatoryReauthOptInConfirmation() {
  MandatoryReauthBubbleControllerImpl::CreateForWebContents(web_contents());
  // TODO(crbug.com/4555994): Pass in the bubble type as a parameter so we
  // enforce that the confirmation bubble is shown.
  MandatoryReauthBubbleControllerImpl::FromWebContents(web_contents())
      ->ReshowBubble();
}

bool ChromePaymentsAutofillClient::IsAutofillPaymentMethodsEnabled() const {
  if (!autofill_payment_methods_supported_ ||
      !prefs::IsAutofillPaymentMethodsEnabled(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext())
              ->GetPrefs())) {
    return false;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAutofillSettingsEnterprisePolicy) &&
      client_->IsAutofillTypeBlockedByPolicy(
          client_->GetLastCommittedPrimaryMainFrameURL(),
          AutofillClient::AutofillPolicyDataCategory::kPayments)) {
    return false;
  }
  return true;
}

void ChromePaymentsAutofillClient::DisablePaymentsAutofill() {
  autofill_payment_methods_supported_ = false;
}

IbanManager* ChromePaymentsAutofillClient::GetIbanManager() {
  if (!iban_manager_) {
    iban_manager_ = std::make_unique<IbanManager>(
        &client_->GetPersonalDataManager().payments_data_manager());
  }
  return iban_manager_.get();
}

IbanAccessManager* ChromePaymentsAutofillClient::GetIbanAccessManager() {
  if (!iban_access_manager_) {
    iban_access_manager_ = std::make_unique<IbanAccessManager>(&client_.get());
  }
  return iban_access_manager_.get();
}

MerchantPromoCodeManager*
ChromePaymentsAutofillClient::GetMerchantPromoCodeManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return MerchantPromoCodeManagerFactory::GetForProfile(profile);
}

void ChromePaymentsAutofillClient::OpenPromoCodeOfferDetailsURL(
    const GURL& url) {
  web_contents()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
                             /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
}

AutofillOfferManager* ChromePaymentsAutofillClient::GetAutofillOfferManager() {
  return AutofillOfferManagerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

void ChromePaymentsAutofillClient::UpdateOfferNotification(
    const AutofillOfferData& offer,
    const OfferNotificationOptions& options) {
  const CreditCard* card = offer.GetEligibleInstrumentIds().empty()
                               ? nullptr
                               : client_->GetPersonalDataManager()
                                     .payments_data_manager()
                                     .GetCreditCardByInstrumentId(
                                         offer.GetEligibleInstrumentIds()[0]);

  if (offer.IsCardLinkedOffer() && !card) {
    return;
  }

  OfferNotificationBubbleControllerImpl::CreateForWebContents(web_contents());
  OfferNotificationBubbleControllerImpl* controller =
      OfferNotificationBubbleControllerImpl::FromWebContents(web_contents());
  controller->ShowOfferNotificationIfApplicable(offer, card, options);
}

void ChromePaymentsAutofillClient::DismissOfferNotification() {
  if (auto* controller = OfferNotificationBubbleControllerImpl::FromWebContents(
          web_contents())) {
    controller->DismissNotification();
  }
}

bool ChromePaymentsAutofillClient::ShowTouchToFillCreditCard(
    base::WeakPtr<TouchToFillPaymentMethodDelegate> delegate,
    base::span<const Suggestion> suggestions) {
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
}

bool ChromePaymentsAutofillClient::ShowTouchToFillIban(
    base::WeakPtr<TouchToFillPaymentMethodDelegate> delegate,
    base::span<const Iban> ibans_to_suggest) {
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
}

bool ChromePaymentsAutofillClient::ShowTouchToFillAffiliatedLoyaltyCard(
    base::WeakPtr<TouchToFillPaymentMethodDelegate> delegate,
    std::vector<LoyaltyCard> loyalty_cards_to_suggest) {
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
}

bool ChromePaymentsAutofillClient::ShowTouchToFillForAllLoyaltyCards(
    base::WeakPtr<TouchToFillPaymentMethodDelegate> delegate,
    std::vector<LoyaltyCard> loyalty_cards_to_suggest) {
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
}

bool ChromePaymentsAutofillClient::OnPurchaseAmountExtracted(
    base::span<const BnplIssuerContext> bnpl_issuer_contexts,
    std::optional<int64_t> extracted_amount,
    bool is_amount_supported_by_any_issuer,
    const std::optional<std::string>& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
}

bool ChromePaymentsAutofillClient::ShowTouchToFillProgress(
    base::OnceClosure cancel_callback) {
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
}

bool ChromePaymentsAutofillClient::ShowTouchToFillBnplIssuers(
    base::span<const BnplIssuerContext> bnpl_issuer_contexts,
    const std::string& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
}

bool ChromePaymentsAutofillClient::ShowTouchToFillBnplTos(
    BnplTosModel bnpl_tos_model,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback) {
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
}

bool ChromePaymentsAutofillClient::ShowTouchToFillError(
    const AutofillErrorDialogContext& context) {
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
}

void ChromePaymentsAutofillClient::HideTouchToFillPaymentMethod() {
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
}

void ChromePaymentsAutofillClient::SetTouchToFillVisible(bool visible) {
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
}

PaymentsDataManager& ChromePaymentsAutofillClient::GetPaymentsDataManager() {
  return client_->GetPersonalDataManager().payments_data_manager();
}

std::unique_ptr<webauthn::InternalAuthenticator>
ChromePaymentsAutofillClient::CreateCreditCardInternalAuthenticator(
    AutofillDriver* driver) {
  auto* cad = static_cast<ContentAutofillDriver*>(driver);
  content::RenderFrameHost* rfh = cad->render_frame_host();
  return std::make_unique<content::InternalAuthenticatorImpl>(rfh);
}

MandatoryReauthManager*
ChromePaymentsAutofillClient::GetOrCreatePaymentsMandatoryReauthManager() {
  if (!payments_mandatory_reauth_manager_) {
    payments_mandatory_reauth_manager_ =
        std::make_unique<MandatoryReauthManager>(&client_.get());
  }

  return payments_mandatory_reauth_manager_.get();
}

SaveAndFillManager* ChromePaymentsAutofillClient::GetSaveAndFillManager() {
  return save_and_fill_manager_.get();
}

void ChromePaymentsAutofillClient::ShowCreditCardLocalSaveAndFillDialog(
    CardSaveAndFillDialogCallback callback) {
  if (!save_and_fill_dialog_controller_) {
    save_and_fill_dialog_controller_ =
        std::make_unique<SaveAndFillDialogControllerImpl>();
  }
  save_and_fill_dialog_controller_->ShowLocalDialog(
      base::BindOnce(&CreateAndShowSaveAndFillDialog,
                     save_and_fill_dialog_controller_->GetWeakPtr(),
                     web_contents()),
      std::move(callback));
}

void ChromePaymentsAutofillClient::ShowCreditCardUploadSaveAndFillDialog(
    const LegalMessageLines& legal_message_lines,
    CardSaveAndFillDialogCallback callback) {
  CHECK(save_and_fill_dialog_controller_);
  save_and_fill_dialog_controller_->ShowUploadDialog(
      std::move(legal_message_lines),
      std::move(callback));
}

void ChromePaymentsAutofillClient::ShowCreditCardSaveAndFillPendingDialog(
    CardSaveAndFillDialogCallback callback) {
  if (!save_and_fill_dialog_controller_) {
    save_and_fill_dialog_controller_ =
        std::make_unique<SaveAndFillDialogControllerImpl>();
  }
  save_and_fill_dialog_controller_->ShowPendingDialog(
      base::BindOnce(&CreateAndShowSaveAndFillDialog,
                     save_and_fill_dialog_controller_->GetWeakPtr(),
                     web_contents()),
      std::move(callback));
}

void ChromePaymentsAutofillClient::HideCreditCardSaveAndFillDialog() {
  if (save_and_fill_dialog_controller_) {
    save_and_fill_dialog_controller_->Dismiss();
  }
}

bool ChromePaymentsAutofillClient::IsTabModalPopup() const {
  tabs::TabInterface* const tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  return tab_interface &&
         tab_interface->GetBrowserWindowInterface()->IsTabModalPopup();
}

BnplStrategy* ChromePaymentsAutofillClient::GetBnplStrategy() {
  if (!bnpl_strategy_) {
    bnpl_strategy_ = std::make_unique<DesktopBnplStrategy>();
  }
  return bnpl_strategy_.get();
}

BnplUiDelegate* ChromePaymentsAutofillClient::GetBnplUiDelegate() {
  if (!bnpl_ui_delegate_) {
    bnpl_ui_delegate_ = std::make_unique<DesktopBnplUiDelegate>(&client_.get());
  }
  return bnpl_ui_delegate_.get();
}

OmniboxAutofillDelegate*
ChromePaymentsAutofillClient::GetOmniboxAutofillDelegate() {
  return omnibox_autofill_delegate_.get();
}

void ChromePaymentsAutofillClient::ShowExpandedOmniboxAutofillChip(
    std::vector<Suggestion> suggestions,
    base::OnceClosure on_chip_shown,
    base::RepeatingCallback<void(base::span<const Suggestion>)>
        on_suggestions_shown,
    base::RepeatingCallback<void(SuggestionHidingReason)> on_suggestions_hidden,
    base::RepeatingCallback<void(const Suggestion&)> did_select_suggestion,
    base::RepeatingClosure did_deselect_suggestion,
    base::RepeatingCallback<
        void(const Suggestion&,
             const AutofillSuggestionDelegate::SuggestionMetadata&)>
        did_accept_suggestion) {
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (!tab_interface) {
    return;
  }
  if (OmniboxAutofillBubbleController* bubble_controller =
          OmniboxAutofillBubbleController::From(*tab_interface)) {
    bubble_controller->Initialize(
        std::move(suggestions), std::move(on_suggestions_shown),
        std::move(on_suggestions_hidden), std::move(did_select_suggestion),
        std::move(did_deselect_suggestion), std::move(did_accept_suggestion));
  }
  if (OmniboxAutofillPageActionController* page_action_controller =
          OmniboxAutofillPageActionController::From(*tab_interface)) {
    page_action_controller->ShowExpandedChip(std::move(on_chip_shown));
  }
}

void ChromePaymentsAutofillClient::HideOmniboxAutofillChip() {
  if (tabs::TabInterface* tab_interface =
          tabs::TabInterface::MaybeGetFromContents(web_contents())) {
    if (OmniboxAutofillPageActionController* page_action_controller =
            OmniboxAutofillPageActionController::From(*tab_interface)) {
      page_action_controller->HideChip();
    }
  }
}

void ChromePaymentsAutofillClient::ShowPaymentsChurnedUsersUI(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure closed_callback) {
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (!tab_interface) {
    return;
  }

  signin::IdentityManager* identity_manager = client_->GetIdentityManager();
  if (!identity_manager) {
    return;
  }

  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      GetPaymentsDataManager().GetAccountInfoForPaymentsServer());
  if (account_info.IsEmpty()) {
    autofill_metrics::LogPaymentsChurnedUsersBubbleShowResult(
        autofill_metrics::PaymentsChurnedUsersBubbleShowResult::
            kNoAccountInfoPresent);
    return;
  }

  if (PaymentsChurnedUsersBubbleController* controller =
          PaymentsChurnedUsersBubbleController::From(*tab_interface)) {
    controller->Show(std::move(accept_callback), std::move(cancel_callback),
                     std::move(closed_callback), std::move(account_info));
  }
}

AutofillProgressDialogControllerImpl*
ChromePaymentsAutofillClient::AutofillProgressDialogControllerForTesting() {
  return autofill_progress_dialog_controller_.get();
}

std::unique_ptr<CardUnmaskPromptControllerImpl>
ChromePaymentsAutofillClient::ExtractCardUnmaskControllerForTesting() {
  return std::move(unmask_controller_);
}
void ChromePaymentsAutofillClient::SetCardUnmaskControllerForTesting(
    std::unique_ptr<CardUnmaskPromptControllerImpl> test_controller) {
  unmask_controller_ = std::move(test_controller);
}

void ChromePaymentsAutofillClient::SetRiskDataForTesting(
    const std::string& risk_data) {
  risk_data_ = risk_data;
}

void ChromePaymentsAutofillClient::SetCachedRiskDataLoadedCallbackForTesting(
    base::OnceCallback<void(const std::string&)>
        cached_risk_data_loaded_callback_for_testing) {
  cached_risk_data_loaded_callback_for_testing_ =
      std::move(cached_risk_data_loaded_callback_for_testing);
}

std::u16string ChromePaymentsAutofillClient::GetAccountHolderName() const {
  if (!web_contents()) {
    return std::u16string();
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile) {
    return std::u16string();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return std::u16string();
  }
  AccountInfo primary_account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  return base::UTF8ToUTF16(primary_account_info.GetFullName().value_or(""));
}

void ChromePaymentsAutofillClient::OnRiskDataLoaded(
    base::OnceCallback<void(const std::string&)> callback,
    base::TimeTicks start_time,
    const std::string& risk_data) {
  autofill_metrics::LogRiskDataLoadingLatency(base::TimeTicks::Now() -
                                              start_time);
  risk_data_ = risk_data;
  std::move(callback).Run(risk_data_);
}

}  // namespace autofill::payments
