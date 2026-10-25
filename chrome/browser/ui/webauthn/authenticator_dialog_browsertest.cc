// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/pin.h"
#include "device/fido/public/features.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "device/fido/public/fido_types.h"
#include "device/fido/public/public_key_credential_descriptor.h"
#include "device/fido/public/public_key_credential_user_entity.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void UpdateModelBeforeStartFlow(
    AuthenticatorRequestDialogModel* model,
    device::FidoRequestHandlerBase::TransportAvailabilityInfo tai,
    bool is_off_the_record) {
  model->request_type = tai.request_type;
  model->resident_key_requirement = tai.resident_key_requirement;
  model->attestation_conveyance_preference =
      tai.attestation_conveyance_preference;
  model->is_off_the_record = is_off_the_record;
  model->platform_has_biometrics = tai.platform_has_biometrics;
}

}  // namespace

// Run with:
//
//   --gtest_filter=BrowserUiTest.Invoke --test-launcher-interactive \
//   --ui=AuthenticatorDialogTest.InvokeUi_${test_name}
//
// where test_name is the second arg to IN_PROC_BROWSER_TEST_F().

class AuthenticatorDialogTest : public DialogBrowserTest {
 public:
  AuthenticatorDialogTest() = default;
  AuthenticatorDialogTest(const AuthenticatorDialogTest&) = delete;
  AuthenticatorDialogTest& operator=(const AuthenticatorDialogTest&) = delete;

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->GetProfile()),
        "user@example.com", signin::ConsentLevel::kSignin);
  }

  void TearDownOnMainThread() override {
    if (controller_) {
      // Close the dialog before the entire browser is torn down.
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kClosed);
    }
    DialogBrowserTest::TearDownOnMainThread();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Web modal dialogs' bounds may exceed the display's work area.
    // https://crbug.com/41419544.
    set_should_verify_dialog_bounds(false);

    content::RenderFrameHost* rfh = browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetPrimaryMainFrame();
    model_ = base::MakeRefCounted<AuthenticatorRequestDialogModel>(rfh);
    model_->relying_party_id = "example.com";
    // Since this code tests UI, it is possible to do everything by configuring
    // just the Model. However, it's easier to do that via a Controller.
    controller_ = std::make_unique<AuthenticatorRequestDialogController>(
        model_.get(), rfh);

    device::FidoRequestHandlerBase::TransportAvailabilityInfo&
        transport_availability =
            controller_->transport_availability_for_testing();
    transport_availability.available_transports = {
        AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kInternal,
        AuthenticatorTransport::kHybrid,
    };

    transport_availability.has_platform_authenticator_credential = device::
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential;
    transport_availability.request_type =
        device::FidoRequestType::kGetAssertion;

    // The dialog should immediately close as soon as it is displayed.
    if (name == "mechanisms" || name == "mechanisms_disabled") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kMechanismSelection);
    } else if (name == "mechanisms_create" ||
               name == "mechanisms_create_disabled") {
      transport_availability.make_credential_attachment =
          device::AuthenticatorAttachment::kAny;
      transport_availability.request_type =
          device::FidoRequestType::kMakeCredential;
      transport_availability.attestation_conveyance_preference =
          device::AttestationConveyancePreference::kNone;
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kMechanismSelection);
    } else if (name == "activate_usb") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kUsbInsertAndActivate);
    } else if (name == "timeout") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kTimedOut);
    } else if (name == "no_available_transports") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kErrorNoAvailableTransports);
    } else if (name == "key_not_registered") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kKeyNotRegistered);
    } else if (name == "key_already_registered") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kKeyAlreadyRegistered);
    } else if (name == "windows_hello_not_enabled") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kErrorWindowsHelloNotEnabled);
    } else if (name == "internal_unrecognized_error") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kErrorInternalUnrecognized);
    } else if (name == "touchid_incognito") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kOffTheRecordInterstitial);
    } else if (name == "set_pin") {
      controller_->CollectPIN(device::pin::PINEntryReason::kSet,
                              device::pin::PINEntryError::kNoError, 6, 0,
                              base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                              device::pin::PINEntryError::kNoError, 6, 8,
                              base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_two_tries_remaining") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                              device::pin::PINEntryError::kWrongPIN, 6, 2,
                              base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_one_try_remaining") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                              device::pin::PINEntryError::kWrongPIN, 6, 1,
                              base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_fallback") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                              device::pin::PINEntryError::kInternalUvLocked, 6,
                              8, base::BindOnce([](std::u16string pin) {}));
    } else if (name == "inline_bio_enrollment") {
      controller_->StartInlineBioEnrollment(base::DoNothing());
      timer_.Start(
          FROM_HERE, base::Seconds(2),
          base::BindLambdaForTesting(
              [&, weak_controller = controller_->GetWeakPtr()] {
                if (!weak_controller || weak_controller->model()->step() !=
                                            AuthenticatorRequestDialogModel::
                                                Step::kInlineBioEnrollment) {
                  return;
                }
                weak_controller->OnSampleCollected(--bio_samples_remaining_);
                if (bio_samples_remaining_ <= 0) {
                  timer_.Stop();
                }
              }));
    } else if (name == "retry_uv") {
      controller_->OnRetryUserVerification(5);
    } else if (name == "retry_uv_two_tries_remaining") {
      controller_->OnRetryUserVerification(2);
    } else if (name == "retry_uv_one_try_remaining") {
      controller_->OnRetryUserVerification(1);
    } else if (name == "force_pin_change") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChange,
                              device::pin::PINEntryError::kNoError, 6, 0,
                              base::BindOnce([](std::u16string pin) {}));
    } else if (name == "force_pin_change_same_as_current") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChange,
                              device::pin::PINEntryError::kSameAsCurrentPIN, 6,
                              0, base::BindOnce([](std::u16string pin) {}));
    } else if (name == "second_tap") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kClientPinTapAgain);
    } else if (name == "soft_block") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kClientPinErrorSoftBlock);
    } else if (name == "hard_block") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kClientPinErrorHardBlock);
    } else if (name == "authenticator_removed") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::
              kClientPinErrorAuthenticatorRemoved);
    } else if (name == "missing_capability") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kMissingCapability);
    } else if (name == "storage_full") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kStorageFull);
    } else if (name == "single_account_select") {
      const std::vector<std::pair<std::string, std::string>> infos = {
          {"verylong@"
           "reallylongreallylongreallylongreallylongreallylongreallylong.com",
           "Very Long String Very Long String Very Long String Very Long "
           "String Very Long String Very Long String "},
      };
      std::vector<device::AuthenticatorGetAssertionResponse> responses;

      for (const auto& info : infos) {
        static const uint8_t kAppParam[32] = {};
        static const uint8_t kSignatureCounter[4] = {};
        device::AuthenticatorData auth_data(kAppParam, 0 /* flags */,
                                            kSignatureCounter, std::nullopt);
        device::AuthenticatorGetAssertionResponse response(
            std::move(auth_data), {10, 11, 12, 13} /* signature */,
            /*transport_used=*/std::nullopt);
        device::PublicKeyCredentialUserEntity user({1, 2, 3, 4});
        user.name = info.first;
        user.display_name = info.second;
        response.credential = device::PublicKeyCredentialDescriptor(
            device::CredentialType::kPublicKey, {1, 2, 3, 4});
        response.user_entity = std::move(user);
        responses.emplace_back(std::move(response));
      }

      controller_->SelectAccount(
          std::move(responses),
          base::BindOnce([](device::AuthenticatorGetAssertionResponse) {}));
    } else if (name == "account_select" || name == "account_select_disabled") {
      // These strings attempt to exercise the encoding of direction and
      // language from https://github.com/w3c/webauthn/pull/1530.

      // lang_and_dir_encoded contains a string with right-to-left and ar-SA
      // tags. It's the UTF-8 encoding of the code points {0xE0001, 0xE0061,
      // 0xE0072, 0xE002D, 0xE0053, 0xE0041, 0x200F, 0xFEA2, 0xFE92, 0xFBFF,
      // 0xFE91, 0x20, 0xFE8E, 0xFEDF, 0xFEAE, 0xFEA4, 0xFEE3, 0xFE8E, 0xFEE7}.
      const std::string lang_and_dir_encoded =
          "\xf3\xa0\x80\x81\xf3\xa0\x81\xa1\xf3\xa0\x81\xb2\xf3\xa0\x80\xad\xf3"
          "\xa0\x81\x93\xf3\xa0\x81\x81\xe2\x80\x8f\xef\xba\xa2\xef\xba\x92\xef"
          "\xaf\xbf\xef\xba\x91\x20\xef\xba\x8e\xef\xbb\x9f\xef\xba\xae\xef\xba"
          "\xa4\xef\xbb\xa3\xef\xba\x8e\xef\xbb\xa7";
      // lang_jp_encoded specifies a kanji with language jp. This is the middle
      // glyph from the example given in
      // https://www.w3.org/TR/string-meta/#capturing-the-text-processing-language.
      // It's the UTF-8 encoding of the code points {0xE0001, 0xE006a, 0xE0070,
      // 0x76f4}.
      const std::string lang_jp_encoded =
          "\xf3\xa0\x80\x81\xf3\xa0\x81\xaa\xf3\xa0\x81\xb0\xe7\x9b\xb4";
      // lang_zh_hant_encoded specifies the same code point as
      // |lang_jp_encoded|, but with the language set to zh-Hant. According to
      // the W3C document referenced above, this should display differently.
      // It's the UTF-8 encoding of the code points {0xE0001, 0xe007a, 0xe0068,
      // 0xe002d, 0xe0048, 0xe0061, 0xe006e, 0xe0074}.
      const std::string lang_zh_hant_encoded =
          "\xf3\xa0\x80\x81\xf3\xa0\x81\xba\xf3\xa0\x81\xa8\xf3\xa0\x80\xad\xf3"
          "\xa0\x81\x88\xf3\xa0\x81\xa1\xf3\xa0\x81\xae\xf3\xa0\x81\xb4";

      const std::vector<std::pair<std::string, std::string>> infos = {
          {"foo@example.com", "Test User 1"},
          {"", "Test User 2"},
          {"", ""},
          {"bat@example.com", "Test User 4"},
          {"user name with\na line break", "display name\nwith a line break"},
          {"encoded@example.com", lang_and_dir_encoded},
          {"encoded2@example.com", lang_jp_encoded},
          {"encoded3@example.com", lang_zh_hant_encoded},
          {"verylong@"
           "reallylongreallylongreallylongreallylongreallylongreallylong.com",
           "Very Long String Very Long String Very Long String Very Long "
           "String Very Long String Very Long String "},
      };
      std::vector<device::AuthenticatorGetAssertionResponse> responses;

      for (const auto& info : infos) {
        static const uint8_t kAppParam[32] = {};
        static const uint8_t kSignatureCounter[4] = {};
        device::AuthenticatorData auth_data(kAppParam, 0 /* flags */,
                                            kSignatureCounter, std::nullopt);
        device::AuthenticatorGetAssertionResponse response(
            std::move(auth_data), {10, 11, 12, 13} /* signature */,
            /*transport_used=*/std::nullopt);
        device::PublicKeyCredentialUserEntity user({1, 2, 3, 4});
        user.name = info.first;
        user.display_name = info.second;
        response.credential = device::PublicKeyCredentialDescriptor(
            device::CredentialType::kPublicKey, {1, 2, 3, 4});
        response.user_entity = std::move(user);
        responses.emplace_back(std::move(response));
      }

      controller_->SelectAccount(
          std::move(responses),
          base::BindOnce([](device::AuthenticatorGetAssertionResponse) {}));

    } else if (name == "create_passkey") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kChromeProfileCreatePasskey);
    }

    UpdateModelBeforeStartFlow(model_.get(), transport_availability,
                               /*is_off_the_record=*/false);
    controller_->StartFlow(std::move(transport_availability));
    if (name.ends_with("_disabled")) {
      model_->ui_disabled_ = true;
      model_->OnSheetModelChanged();
    }
  }

 private:
  scoped_refptr<AuthenticatorRequestDialogModel> model_;
  std::unique_ptr<AuthenticatorRequestDialogController> controller_;
  base::RepeatingTimer timer_;
  int bio_samples_remaining_ = 5;
};

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_force_pin_change) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_force_pin_change_same_as_current) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_mechanisms) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_mechanisms_create) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_mechanisms_disabled) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_mechanisms_create_disabled) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_activate_usb) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_timeout) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_no_available_transports) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_key_not_registered) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_key_already_registered) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_windows_hello_not_enabled) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_internal_unrecognized_error) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_touchid) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_touchid_incognito) {
  ShowAndVerifyUi();
}
#endif  // BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_set_pin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_get_pin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_get_pin_two_tries_remaining) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_get_pin_one_try_remaining) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_get_pin_fallback) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_inline_bio_enrollment) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_retry_uv) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_retry_uv_two_tries_remaining) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_retry_uv_one_try_remaining) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_second_tap) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_soft_block) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_hard_block) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_authenticator_removed) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_missing_capability) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_storage_full) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_resident_credential_confirm) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_single_account_select) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_account_select) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_account_select_disabled) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_create_passkey) {
  ShowAndVerifyUi();
}
