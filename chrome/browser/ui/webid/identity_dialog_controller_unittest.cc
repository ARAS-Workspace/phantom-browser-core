// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/browser/webid/federated_embedder_login_request.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const std::vector<content::IdentityRequestDialogDisclosureField>
    kDefaultPermissions = {
        content::IdentityRequestDialogDisclosureField::kName,
        content::IdentityRequestDialogDisclosureField::kEmail,
        content::IdentityRequestDialogDisclosureField::kPicture};

}  // namespace

constexpr char kAccountId[] = "account_id1";
constexpr char16_t kTopFrameEtldPlusOne[] = u"top-frame-example.com";
constexpr char kIdpEtldPlusOne[] = "idp-example.com";

// Mock version of AccountSelectionView for injection during tests.
class MockAccountSelectionView : public AccountSelectionView {
 public:
  MockAccountSelectionView() : AccountSelectionView(/*delegate=*/nullptr) {}
  ~MockAccountSelectionView() override = default;

  MockAccountSelectionView(const MockAccountSelectionView&) = delete;
  MockAccountSelectionView& operator=(const MockAccountSelectionView&) = delete;

  MOCK_METHOD(void, OnPageActionClicked, (), (override));

  MOCK_METHOD(
      bool,
      Show,
      (const content::RelyingPartyData& rp_data,
       const std::vector<IdentityProviderDataPtr>& identity_provider_data,
       const std::vector<IdentityRequestAccountPtr>& accounts,
       blink::mojom::RpMode rp_mode,
       const std::vector<IdentityRequestAccountPtr>& new_accounts),
      (override));

  MOCK_METHOD(bool,
              ShowFailureDialog,
              (const content::RelyingPartyData& rp_data,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const content::IdentityProviderMetadata& idp_metadata),
              (override));

  MOCK_METHOD(bool,
              ShowErrorDialog,
              (const content::RelyingPartyData& rp_data,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const content::IdentityProviderMetadata& idp_metadata,
               const std::optional<TokenError>& error),
              (override));

  MOCK_METHOD(bool,
              ShowLoadingDialog,
              (const content::RelyingPartyData& rp_data,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode),
              (override));

  MOCK_METHOD(bool,
              ShowVerifyingDialog,
              (const content::RelyingPartyData&,
               const IdentityProviderDataPtr&,
               const IdentityRequestAccountPtr&,
               Account::SignInMode sign_in_mode,
               blink::mojom::RpMode),
              (override));

  MOCK_METHOD(std::string, GetTitle, (), (const, override));

  MOCK_METHOD(std::optional<std::string>, GetSubtitle, (), (const, override));

  MOCK_METHOD(void, ShowUrl, (LinkType type, const GURL& url), (override));

  MOCK_METHOD(content::WebContents*,
              ShowModalDialog,
              (const GURL& url,
               blink::mojom::RpMode rp_mode,
               content::IdentityRequestDialogController::ShownModalAsyncCallback
                   on_shown_async),
              (override));

  MOCK_METHOD(void, CloseModalDialog, (), (override));

  MOCK_METHOD(content::WebContents*, GetRpWebContents, (), (override));

  MOCK_METHOD(void, SetCanShowUi, (bool), (override));
};

class IdentityDialogControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  IdentityDialogControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~IdentityDialogControllerTest() override = default;
  IdentityDialogControllerTest(IdentityDialogControllerTest&) = delete;
  IdentityDialogControllerTest& operator=(IdentityDialogControllerTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL(permissions::MockPermissionRequest::kDefaultOrigin));
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  void WaitForBubbleToBeShown(permissions::PermissionRequestManager* manager) {
    manager->DocumentOnLoadCompletedInPrimaryMainFrame();
    task_environment()->RunUntilIdle();
  }

  void Accept(permissions::PermissionRequestManager* manager) {
    manager->Accept(/*prompt_options=*/std::monostate());
    task_environment()->RunUntilIdle();
  }

  void Deny(permissions::PermissionRequestManager* manager) {
    manager->Deny(/*prompt_options=*/std::monostate());
    task_environment()->RunUntilIdle();
  }

  void Dismiss(permissions::PermissionRequestManager* manager) {
    manager->Dismiss(/*prompt_options=*/std::monostate());
    task_environment()->RunUntilIdle();
  }

  std::vector<IdentityRequestAccountPtr> CreateAccount() {
    return {base::MakeRefCounted<Account>(
        kAccountId, "", "", "", "", "", GURL(), "", "",
        /*potentially_approved_origin_hashes=*/std::vector<std::string>(),
        /*login_hints=*/std::vector<std::string>(),
        /*domain_hints=*/std::vector<std::string>(),
        /*labels=*/std::vector<std::string>(),
        /*idp_claimed_login_state=*/
        content::IdentityRequestAccount::LoginState::kSignIn,
        /*browser_trusted_login_state=*/
        content::IdentityRequestAccount::LoginState::kSignUp)};
  }

  IdentityProviderDataPtr CreateIdentityProviderData(
      std::vector<IdentityRequestAccountPtr>& accounts) {
    IdentityProviderDataPtr idp_data =
        base::MakeRefCounted<content::IdentityProviderData>(
            kIdpEtldPlusOne, content::IdentityProviderMetadata(),
            content::ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
            blink::mojom::RpContext::kSignIn, /*format=*/std::nullopt,
            kDefaultPermissions,
            /*has_login_status_mismatch=*/false);
    for (auto& account : accounts) {
      account->identity_provider = idp_data;
    }
    return idp_data;
  }

  void ShowAccountsDialog(
      IdentityDialogController* controller,
      blink::mojom::RpMode rp_mode,
      DismissCallback dismiss_callback = base::DoNothing()) {
    accounts_ = CreateAccount();
    IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts_);
    controller->ShowAccountsDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne,
                                  /*iframe_for_display=*/u""),
        {idp_data}, accounts_, /*filtered_accounts=*/{}, rp_mode,
        /*on_selected=*/base::DoNothing(),
        /*on_add_account=*/base::DoNothing(), std::move(dismiss_callback),
        /*accounts_displayed_callback=*/base::DoNothing());
  }

 protected:
  std::vector<IdentityRequestAccountPtr> accounts_;
};

TEST_F(IdentityDialogControllerTest, Accept) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(true)).WillOnce(testing::Return());
  controller->RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Accept(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}

TEST_F(IdentityDialogControllerTest, Deny) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(false)).WillOnce(testing::Return());
  controller->RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Deny(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}

TEST_F(IdentityDialogControllerTest, Dismiss) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(false)).WillOnce(testing::Return());
  controller->RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Dismiss(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}

// Test that selecting an account in button mode, and then dismissing it should
// run the dismiss callback.
TEST_F(IdentityDialogControllerTest, OnAccountSelectedButtonCallsDismiss) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  controller->SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);

  // Dismiss callback should run once.
  base::MockCallback<DismissCallback> dismiss_callback;
  EXPECT_CALL(dismiss_callback, Run).WillOnce(testing::Return());

  ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kActive,
                     dismiss_callback.Get());

  // User selects an account, and then dismisses it. The expectation set for
  // dismiss callback should pass.
  controller->OnAccountSelected(GURL(kIdpEtldPlusOne), accounts[0]->id,
                                *accounts[0]->idp_claimed_login_state);
  controller->OnDismiss(IdentityDialogController::DismissReason::kOther);
}

// Test that selecting an account in widget, and then dismissing it should not
// run the dismiss callback.
TEST_F(IdentityDialogControllerTest, OnAccountSelectedWidgetResetsDismiss) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  controller->SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);

  // Dismiss callback should not be run.
  base::MockCallback<DismissCallback> dismiss_callback;
  EXPECT_CALL(dismiss_callback, Run).Times(0);

  ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive,
                     dismiss_callback.Get());

  // User selects an account, and then dismisses it. The expectation set for
  // dismiss callback should pass.
  controller->OnAccountSelected(GURL(kIdpEtldPlusOne), accounts[0]->id,
                                *accounts[0]->idp_claimed_login_state);
  controller->OnDismiss(IdentityDialogController::DismissReason::kOther);
}

// Crash test for crbug.com/358302105.
TEST_F(IdentityDialogControllerTest, NoTabDoesNotCrash) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);

  ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kActive);
}

TEST_F(IdentityDialogControllerTest,
       AccountSelectionInvokedWhenShouldShowFedCmUiIsFalse) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  controller->SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  // Set up ActorLoginRequest to make ShouldShowFedCmUi returns false.
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  content::webid::FederatedEmbedderLoginRequest::Set(
      web_contents(), idp_origin, kAccountId, base::DoNothing());

  std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
  accounts[0]->idp_claimed_login_state =
      content::IdentityRequestAccount::LoginState::kSignIn;
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);
  idp_data->idp_metadata.config_url = idp_url;

  base::MockCallback<AccountSelectionCallback> on_selected;
  EXPECT_CALL(on_selected, Run(idp_url, kAccountId, true)).Times(1);

  // ShowAccountsDialog should return true and trigger the callback immediately
  // without showing UI.
  EXPECT_TRUE(controller->ShowAccountsDialog(
      content::RelyingPartyData(kTopFrameEtldPlusOne,
                                /*iframe_for_display=*/u""),
      {idp_data}, accounts, /*filtered_accounts=*/{},
      blink::mojom::RpMode::kActive, on_selected.Get(),
      /*on_add_account=*/base::DoNothing(),
      /*dismiss_callback=*/base::DoNothing(),
      /*accounts_displayed_callback=*/base::DoNothing()));
}

TEST_F(IdentityDialogControllerTest,
       TokenReceivedCallbackInvokedWhenAccountNotSignedInOrMissing) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  controller->SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";

  // Case 1: Account is missing.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        result_callback;
    EXPECT_CALL(result_callback,
                Run(content::webid::FederatedLoginResult::kAccountNotLoggedIn))
        .Times(1);

    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, result_callback.Get());

    // Create an account with different ID.
    std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
    accounts[0]->id = "other_account";
    IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);
    idp_data->idp_metadata.config_url = idp_url;

    EXPECT_FALSE(controller->ShowAccountsDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne,
                                  /*iframe_for_display=*/u""),
        {idp_data}, accounts, /*filtered_accounts=*/{},
        blink::mojom::RpMode::kActive,
        /*on_selected=*/base::DoNothing(),
        /*on_add_account=*/base::DoNothing(),
        /*dismiss_callback=*/base::DoNothing(),
        /*accounts_displayed_callback=*/base::DoNothing()));
  }

  // Case 2: Account exists but is not signed in.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        result_callback;
    EXPECT_CALL(result_callback,
                Run(content::webid::FederatedLoginResult::kAccountIsSignUp))
        .Times(1);

    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, result_callback.Get());

    std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
    // Ensure account matches.
    accounts[0]->id = account_id;
    // Ensure not signed in.
    accounts[0]->idp_claimed_login_state =
        content::IdentityRequestAccount::LoginState::kSignUp;
    accounts[0]->browser_trusted_login_state =
        content::IdentityRequestAccount::LoginState::kSignUp;

    IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);
    idp_data->idp_metadata.config_url = idp_url;

    EXPECT_FALSE(controller->ShowAccountsDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne,
                                  /*iframe_for_display=*/u""),
        {idp_data}, accounts, /*filtered_accounts=*/{},
        blink::mojom::RpMode::kActive,
        /*on_selected=*/base::DoNothing(),
        /*on_add_account=*/base::DoNothing(),
        /*dismiss_callback=*/base::DoNothing(),
        /*accounts_displayed_callback=*/base::DoNothing()));
  }

  // Case 3: Account is filtered out.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        result_callback;
    EXPECT_CALL(result_callback,
                Run(content::webid::FederatedLoginResult::kAccountNotAvailable))
        .Times(1);

    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, result_callback.Get());

    // Create an account with matching ID but in filtered_accounts list.
    std::vector<IdentityRequestAccountPtr> filtered_accounts = CreateAccount();
    filtered_accounts[0]->id = account_id;
    filtered_accounts[0]->idp_claimed_login_state =
        content::IdentityRequestAccount::LoginState::kSignIn;
    filtered_accounts[0]->browser_trusted_login_state =
        content::IdentityRequestAccount::LoginState::kSignIn;

    IdentityProviderDataPtr idp_data =
        CreateIdentityProviderData(filtered_accounts);
    idp_data->idp_metadata.config_url = idp_url;

    EXPECT_FALSE(controller->ShowAccountsDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne,
                                  /*iframe_for_display=*/u""),
        {idp_data}, /*accounts=*/{}, filtered_accounts,
        blink::mojom::RpMode::kActive,
        /*on_selected=*/base::DoNothing(),
        /*on_add_account=*/base::DoNothing(),
        /*dismiss_callback=*/base::DoNothing(),
        /*accounts_displayed_callback=*/base::DoNothing()));
  }

  // Case 4: ShowFailureDialog with filtered_accounts.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        result_callback;
    EXPECT_CALL(result_callback,
                Run(content::webid::FederatedLoginResult::kAccountNotAvailable))
        .Times(1);

    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, result_callback.Get());

    // Create an account with matching ID but in filtered_accounts list.
    std::vector<IdentityRequestAccountPtr> filtered_accounts = CreateAccount();
    filtered_accounts[0]->id = account_id;
    filtered_accounts[0]->idp_claimed_login_state =
        content::IdentityRequestAccount::LoginState::kSignIn;
    filtered_accounts[0]->browser_trusted_login_state =
        content::IdentityRequestAccount::LoginState::kSignIn;

    IdentityProviderDataPtr idp_data =
        CreateIdentityProviderData(filtered_accounts);
    idp_data->idp_metadata.config_url = idp_url;

    EXPECT_FALSE(controller->ShowFailureDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne,
                                  /*iframe_for_display=*/u""),
        kIdpEtldPlusOne, blink::mojom::RpContext::kSignIn,
        blink::mojom::RpMode::kActive, idp_data->idp_metadata,
        filtered_accounts,
        /*dismiss_callback=*/base::DoNothing(),
        /*login_callback=*/base::DoNothing()));
  }
}

TEST_F(IdentityDialogControllerTest, ActorLoginContinuation) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  controller->SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";

  // Test that showing modal dialog results in kContinuation.
  base::MockCallback<
      base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
      result_callback;
  EXPECT_CALL(result_callback,
              Run(content::webid::FederatedLoginResult::kContinuation))
      .Times(1);

  content::webid::FederatedEmbedderLoginRequest::Set(
      web_contents(), idp_origin, account_id, result_callback.Get());

  controller->ShowModalDialog(GURL("https://idp.example/login"),
                              blink::mojom::RpMode::kActive, base::DoNothing(),
                              base::DoNothing(), base::DoNothing());
}

TEST_F(IdentityDialogControllerTest,
       PassiveAccountSelectionWithEmbedderLoginRequest) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  auto mock_view = std::make_unique<MockAccountSelectionView>();
  MockAccountSelectionView* mock_view_ptr = mock_view.get();
  controller->SetAccountSelectionViewForTesting(std::move(mock_view));

  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  content::webid::FederatedEmbedderLoginRequest::Set(
      web_contents(), idp_origin, kAccountId, base::DoNothing());

  accounts_ = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts_);
  idp_data->idp_metadata.config_url = idp_url;

  // Should auto select and not show UI.
  EXPECT_CALL(*mock_view_ptr, Show).Times(0);

  base::MockCallback<AccountSelectionCallback> on_selected;
  EXPECT_CALL(on_selected, Run(idp_url, kAccountId, true)).Times(1);

  EXPECT_TRUE(controller->ShowAccountsDialog(
      content::RelyingPartyData(kTopFrameEtldPlusOne,
                                /*iframe_for_display=*/u""),
      {idp_data}, accounts_, /*filtered_accounts=*/{},
      blink::mojom::RpMode::kPassive, on_selected.Get(),
      /*on_add_account=*/base::DoNothing(),
      /*dismiss_callback=*/base::DoNothing(),
      /*accounts_displayed_callback=*/base::DoNothing()));
}
