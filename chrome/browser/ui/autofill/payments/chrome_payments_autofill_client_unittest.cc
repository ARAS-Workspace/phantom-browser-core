// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include <optional>
#include <vector>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl_test_api.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_page_action_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"  // nogncheck
#include "components/autofill/core/browser/payments/desktop_bnpl_strategy.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

using ::autofill::test::CreateLoyaltyCard;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Ne;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;

namespace autofill {

class MockSaveCardBubbleController : public SaveCardBubbleControllerImpl {
 public:
  explicit MockSaveCardBubbleController(content::WebContents* web_contents)
      : SaveCardBubbleControllerImpl(web_contents) {}
  ~MockSaveCardBubbleController() override = default;

  MOCK_METHOD(void,
              OfferLocalSave,
              (const CreditCard&,
               payments::PaymentsAutofillClient::SaveCreditCardOptions,
               payments::PaymentsAutofillClient::LocalSaveCardPromptCallback),
              (override));
  MOCK_METHOD(
      void,
      ShowConfirmationBubbleView,
      (bool,
       bool,
       std::optional<
           payments::PaymentsAutofillClient::OnConfirmationClosedCallback>),
      (override));
  MOCK_METHOD(void, HideSaveCardBubble, (), (override));
};

class MockVirtualCardEnrollBubbleController
    : public VirtualCardEnrollBubbleControllerImpl {
 public:
  explicit MockVirtualCardEnrollBubbleController(
      content::WebContents* web_contents)
      : VirtualCardEnrollBubbleControllerImpl(web_contents) {}
  ~MockVirtualCardEnrollBubbleController() override = default;

  MOCK_METHOD(void,
              ShowConfirmationBubbleView,
              (payments::PaymentsAutofillClient::PaymentsRpcResult),
              (override));
};

class MockPaymentsChurnedUsersBubbleController
    : public PaymentsChurnedUsersBubbleController {
 public:
  explicit MockPaymentsChurnedUsersBubbleController(
      tabs::TabInterface& tab_interface,
      content::WebContents* web_contents)
      : PaymentsChurnedUsersBubbleController(tab_interface, web_contents) {}
  ~MockPaymentsChurnedUsersBubbleController() override = default;

  MOCK_METHOD(void,
              Show,
              (base::OnceClosure accept_callback,
               base::OnceClosure cancel_callback,
               base::OnceClosure closed_callback,
               AccountInfo account_info),
              (override));
};

class ChromePaymentsAutofillClientTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromePaymentsAutofillClientTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnablePrefetchingRiskDataForRetrieval);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ChromeAutofillClient::CreateForWebContents(web_contents());
    auto mock_virtual_card_bubble_controller =
        std::make_unique<MockVirtualCardEnrollBubbleController>(web_contents());
    const auto* user_data_key =
        mock_virtual_card_bubble_controller->UserDataKey();
    web_contents()->SetUserData(user_data_key,
                                std::move(mock_virtual_card_bubble_controller));
    auto mock_save_card_bubble_controller =
        std::make_unique<MockSaveCardBubbleController>(web_contents());
    user_data_key = mock_save_card_bubble_controller->UserDataKey();
    web_contents()->SetUserData(user_data_key,
                                std::move(mock_save_card_bubble_controller));
  }

  ChromeAutofillClient* client() {
    return ChromeAutofillClient::FromWebContentsForTesting(web_contents());
  }

  payments::ChromePaymentsAutofillClient* chrome_payments_client() {
    return static_cast<payments::ChromePaymentsAutofillClient*>(
        client()->GetPaymentsAutofillClient());
  }

  MockVirtualCardEnrollBubbleController& virtual_card_bubble_controller() {
    return static_cast<MockVirtualCardEnrollBubbleController&>(
        *VirtualCardEnrollBubbleController::GetOrCreate(web_contents()));
  }
  MockSaveCardBubbleController& save_card_bubble_controller() {
    return static_cast<MockSaveCardBubbleController&>(
        *SaveCardBubbleController::GetOrCreate(web_contents()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/410047802): Disable test on Linux TSan due to flakiness/issue.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_ShowSaveCreditCardLocally_CallsOfferLocalSave \
  DISABLED_ShowSaveCreditCardLocally_CallsOfferLocalSave
#else
#define MAYBE_ShowSaveCreditCardLocally_CallsOfferLocalSave \
  ShowSaveCreditCardLocally_CallsOfferLocalSave
#endif  // BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
TEST_F(ChromePaymentsAutofillClientTest,
       MAYBE_ShowSaveCreditCardLocally_CallsOfferLocalSave) {
  EXPECT_CALL(save_card_bubble_controller(), OfferLocalSave);

  chrome_payments_client()->ShowSaveCreditCardLocally(
      CreditCard(),
      payments::ChromePaymentsAutofillClient::SaveCreditCardOptions(),
      base::DoNothing());
}

// Test that calling `CreditCardUploadCompleted` calls
// SaveCardBubbleControllerImpl::ShowConfirmationBubbleView on card upload
// success.
TEST_F(ChromePaymentsAutofillClientTest,
       CreditCardUploadCompletedSuccess_CallsShowConfirmationBubbleView) {
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(/*card_saved=*/true,
                                         /*is_for_save_and_fill=*/true, _));
  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::nullopt);
}

// Test that calling `CreditCardUploadCompleted` calls
// SaveCardBubbleControllerImpl::ShowConfirmationBubbleView on card upload
// failure.
TEST_F(ChromePaymentsAutofillClientTest,
       CreditCardUploadCompletedFailure_CallsShowConfirmationBubbleView) {
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(/*card_saved=*/false,
                                         /*is_for_save_and_fill=*/false, _));
  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure,
      std::nullopt);
}

// Test that calling `CreditCardUploadCompleted` does not show failure
// confirmation on card upload client-side timeout.
TEST_F(
    ChromePaymentsAutofillClientTest,
    CreditCardUploadCompletedClientSideTimeout_CallsShowConfirmationBubbleView) {
  EXPECT_CALL(save_card_bubble_controller(), HideSaveCardBubble());
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(/*card_saved=*/false,
                                         /*is_for_save_and_fill=*/false, _))
      .Times(0);
  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kClientSideTimeout,
      std::nullopt);
}

// Verify that the confirmation bubble view is shown after virtual card
// enrollment is completed.
TEST_F(ChromePaymentsAutofillClientTest,
       VirtualCardEnrollCompleted_ShowsConfirmation) {
  EXPECT_CALL(
      virtual_card_bubble_controller(),
      ShowConfirmationBubbleView(
          payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess));
  chrome_payments_client()->VirtualCardEnrollCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
}

// Test that there is always an PaymentsWindowManager present if attempted
// to be retrieved.
TEST_F(ChromePaymentsAutofillClientTest, GetPaymentsWindowManager) {
    EXPECT_NE(chrome_payments_client()->GetPaymentsWindowManager(), nullptr);
}

TEST_F(ChromePaymentsAutofillClientTest, RiskDataCaching_DataCached) {
  base::MockCallback<base::OnceCallback<void(const std::string&)>> callback1;
  base::MockCallback<base::OnceCallback<void(const std::string&)>> callback2;
  chrome_payments_client()->SetCachedRiskDataLoadedCallbackForTesting(
      callback1.Get());
  chrome_payments_client()->SetRiskDataForTesting("risk_data");

  EXPECT_CALL(callback1, Run("risk_data")).Times(1);
  EXPECT_CALL(callback2, Run).Times(0);

  chrome_payments_client()->LoadRiskData(callback2.Get());
}

// Test that BNPL strategy is created and returned correctly.
TEST_F(ChromePaymentsAutofillClientTest, GetBnplStrategy) {
  payments::BnplStrategy* strategy =
      chrome_payments_client()->GetBnplStrategy();
  ASSERT_NE(strategy, nullptr);

  // Test that the same instance is returned on subsequent calls.
  EXPECT_EQ(strategy, chrome_payments_client()->GetBnplStrategy());
}

// Test that BNPL UI delegate is created and returned correctly.
TEST_F(ChromePaymentsAutofillClientTest, GetBnplUiDelegate) {
  payments::BnplUiDelegate* ui_delegate =
      chrome_payments_client()->GetBnplUiDelegate();
  ASSERT_NE(ui_delegate, nullptr);

  // Test that the same instance is returned on subsequent calls.
  EXPECT_EQ(ui_delegate, chrome_payments_client()->GetBnplUiDelegate());
}

// Test that `DisablePaymentsAutofill` correctly disables the client's support
// for autofill payment methods.
TEST_F(ChromePaymentsAutofillClientTest, DisablePaymentsAutofill) {
  EXPECT_TRUE(chrome_payments_client()->IsAutofillPaymentMethodsEnabled());

  chrome_payments_client()->DisablePaymentsAutofill();

  EXPECT_FALSE(chrome_payments_client()->IsAutofillPaymentMethodsEnabled());
}

TEST_F(ChromePaymentsAutofillClientTest,
       IsAutofillPaymentMethodsEnabled_BlockedByPolicy) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnableAutofillSettingsEnterprisePolicy);
  NavigateAndCommit(GURL("https://example.com"));

  EXPECT_TRUE(chrome_payments_client()->IsAutofillPaymentMethodsEnabled());

  profile()->GetPrefs()->Set(
      prefs::kAutofillTypesBlocked,
      base::test::ParseJson(
          R"([{"url_pattern": "https://example.com", "blocked_types": ["payments"]}])"));

  EXPECT_FALSE(chrome_payments_client()->IsAutofillPaymentMethodsEnabled());
}

class ChromePaymentsAutofillIOSPromoClientTest
    : public ChromePaymentsAutofillClientTest {
 public:
  ChromePaymentsAutofillIOSPromoClientTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnablePrefetchingRiskDataForRetrieval);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that calling `CreditCardUploadCompleted` still calls
// SaveCardBubbleControllerImpl::ShowConfirmationBubbleView on card upload
// success as callback, after failing to show the iOS promo.
TEST_F(ChromePaymentsAutofillIOSPromoClientTest,
       IOSPaymentPromoFailedToShow_CallsShowConfirmationBubbleView) {
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(/*card_saved=*/true,
                                         /*is_for_save_and_fill=*/true, _));
  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::nullopt);
}

class ChromePaymentsAutofillClientOmniboxTest
    : public ChromePaymentsAutofillClientTest {
 public:
  ChromePaymentsAutofillClientOmniboxTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableOmniboxAutofill);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that Omnibox Autofill delegate is created and returned correctly.
TEST_F(ChromePaymentsAutofillClientOmniboxTest, GetOmniboxAutofillDelegate) {
  OmniboxAutofillDelegate* omnibox_autofill_delegate =
      chrome_payments_client()->GetOmniboxAutofillDelegate();
  ASSERT_NE(omnibox_autofill_delegate, nullptr);

  // Test that the same instance is returned on subsequent calls.
  EXPECT_EQ(omnibox_autofill_delegate,
            chrome_payments_client()->GetOmniboxAutofillDelegate());
}

TEST_F(ChromePaymentsAutofillClientOmniboxTest,
       ShowExpandedOmniboxAutofillChip) {
  tabs::MockTabInterface mock_tab_interface;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(mock_tab_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));

  page_actions::MockPageActionController mock_page_action_controller;
  OmniboxAutofillPageActionController omnibox_controller(
      mock_tab_interface, mock_page_action_controller);

  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab_interface);

  EXPECT_CALL(mock_page_action_controller, Show(kActionAutofillPayment))
      .Times(1);
  EXPECT_CALL(mock_page_action_controller,
              ShowSuggestionChip(kActionAutofillPayment, _))
      .Times(1);

  chrome_payments_client()->ShowExpandedOmniboxAutofillChip(
      /*suggestions=*/{},
      /*on_chip_shown=*/base::DoNothing(),
      /*on_suggestions_shown=*/base::DoNothing(),
      /*on_suggestions_hidden=*/base::DoNothing(),
      /*did_select_suggestion=*/base::DoNothing(),
      /*did_deselect_suggestion=*/base::DoNothing(),
      /*did_accept_suggestion=*/base::DoNothing());
}

TEST_F(ChromePaymentsAutofillClientOmniboxTest, HideOmniboxAutofillChip) {
  tabs::MockTabInterface mock_tab_interface;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(mock_tab_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));

  page_actions::MockPageActionController mock_page_action_controller;
  OmniboxAutofillPageActionController omnibox_controller(
      mock_tab_interface, mock_page_action_controller);

  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab_interface);

  EXPECT_CALL(mock_page_action_controller,
              HideSuggestionChip(kActionAutofillPayment))
      .Times(1);
  EXPECT_CALL(mock_page_action_controller, Hide(kActionAutofillPayment))
      .Times(1);

  chrome_payments_client()->HideOmniboxAutofillChip();
}

TEST_F(ChromePaymentsAutofillClientTest,
       ShowPaymentsChurnedUsersUI_WithAccountInfo) {
  tabs::MockTabInterface mock_tab_interface;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(mock_tab_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));

  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab_interface);

  MockPaymentsChurnedUsersBubbleController controller(mock_tab_interface,
                                                      web_contents());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@example.com", signin::ConsentLevel::kSignin);
  signin::UpdateAccountInfoForAccount(
      identity_manager, signin::WithGeneratedUserInfo(account_info, "Test"));

  EXPECT_CALL(controller, Show).Times(1);

  chrome_payments_client()->ShowPaymentsChurnedUsersUI(
      base::DoNothing(), base::DoNothing(), base::DoNothing());
}

TEST_F(ChromePaymentsAutofillClientTest,
       ShowPaymentsChurnedUsersUI_NoAccountInfo) {
  tabs::MockTabInterface mock_tab_interface;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(mock_tab_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));

  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab_interface);

  MockPaymentsChurnedUsersBubbleController controller(mock_tab_interface,
                                                      web_contents());

  EXPECT_CALL(controller, Show).Times(0);

  chrome_payments_client()->ShowPaymentsChurnedUsersUI(
      base::DoNothing(), base::DoNothing(), base::DoNothing());
}

}  // namespace autofill
