// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <vector>

#include "base/base64url.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/fake_password_credential_fetcher.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/public/features.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "device/fido/public/fido_types.h"
#include "device/fido/public/public_key_credential_user_entity.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/common/extension_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

static constexpr uint8_t kCredentialID[] = {1, 2,  3,  4,  5,  6,  7,  8,
                                            9, 10, 11, 12, 13, 14, 15, 16};

static constexpr char kSignalUnknownCredentialId[] = R"(
PublicKeyCredential.signalUnknownCredential({
  rpId: "www.example.com",
  credentialId: "$1",
}).then(c => 'webauthn: OK', e => 'error ' + e);
)";

// The hints parameter here contains nonsense values (which should be ignored)
// and lists `security-key` and `hybrid` (more than once).
//
// According to the standard,
//
// "Hints are provided in order of decreasing preference so, if two hints are
// contradictory, the first one controls. [...] If the same hint appears more
// than once, its second and later appearances are ignored."
//
// In practice, Chromium will only consider the first recognised hint and ignore
// the rest for the purposes of configuring the UI.
// For cases where Chromium delegates WebAuthn to the OS (e.g. Windows), unknown
// hints are filtered, but they are otherwise passed as received.
static constexpr char kMakeCredentialWithHints[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    hints: ["nonsense", "hybrid", "security-key", "hybrid", "nonsense"],
    userVerification: 'discouraged',
  }}).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

// This file tests WebAuthn features that depend on specific //chrome behaviour.
// Tests that don't depend on that should go into
// content/browser/webauth/webauth_browsertest.cc.
class WebAuthnBrowserTest : public CertVerifierBrowserTest {
 public:
  WebAuthnBrowserTest() = default;
  WebAuthnBrowserTest(const WebAuthnBrowserTest&) = delete;
  WebAuthnBrowserTest& operator=(const WebAuthnBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUp() override {
    https_server_.SetCertHostnames({"www.example.com"});
    ASSERT_TRUE(https_server_.InitializeAndListen());
    CertVerifierBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();

    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_server_.StartAcceptingConnections();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Allowlist all certs for the HTTPS server.
    mock_cert_verifier()->set_default_result(net::OK);
  }

 protected:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

static constexpr char kGetAssertionCredID1234[] = R"((() => {
  let cred_id = new Uint8Array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
  return navigator.credentials.get({ publicKey: {
    challenge: cred_id,
    timeout: 10000,
    userVerification: 'discouraged',
    allowCredentials: [{type: 'public-key', id: cred_id}],
  }}).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest, ChromeExtensions) {
  // Test that WebAuthn works inside of Chrome extensions. WebAuthn is based on
  // Relying Party IDs, which are domain names. But Chrome extensions don't have
  // domain names therefore the origin is used in their case.
  //
  // This test creates and installs an extension and then loads an HTML page
  // from inside that extension. A WebAuthn call is injected into that context
  // and it should get an assertion from a credential that's injected into the
  // virtual authenticator, scoped to the origin string.
  base::ScopedAllowBlockingForTesting allow_blocking;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  static constexpr char kPageFile[] = "page.html";

  static constexpr char kContents[] = R"(
<html>
  <head>
    <title>WebAuthn in extensions test</title>
  </head>
  <body>
  </body>
</html>
)";
  WriteFile(temp_dir.GetPath().AppendASCII(kPageFile), kContents);

  static constexpr char kExtensionSite[] = "https://extension-site.com/";
  static constexpr char kWebAccessibleResources[] =
      R"([{
            "resources": ["page.html"],
            "matches": ["*://*/*"]
         }])";

  extensions::ExtensionBuilder builder("test");
  builder.SetPath(temp_dir.GetPath())
      .SetVersion("1.0")
      .AddHostPermission(kExtensionSite)
      .SetLocation(extensions::mojom::ManifestLocation::kExternalPolicyDownload)
      .SetManifestKey("web_accessible_resources",
                      base::test::ParseJson(kWebAccessibleResources));

  scoped_refptr<const extensions::Extension> extension = builder.Build();
  extensions::ExtensionRegistrar::Get(browser()->GetProfile())
      ->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(), 0);

  auto virtual_device_factory =
      std::make_unique<device::test::VirtualFidoDeviceFactory>();
  const GURL url = extension->GetResourceURL(kPageFile);
  auto extension_id = url.GetHost();
  virtual_device_factory->mutable_state()->InjectRegistration(
      kCredentialID, "chrome-extension://" + extension_id);

  content::ScopedAuthenticatorEnvironmentForTesting auth_env(
      std::move(virtual_device_factory));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(
      "webauthn: OK",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kGetAssertionCredID1234));

  static constexpr char kMakeCredentialCrossDomainWithHostPerms[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { id: "extension-site.com", name: "" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
  }}).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

  // This should work as the extension has host permissions over the site.
  EXPECT_EQ(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kMakeCredentialCrossDomainWithHostPerms)
          .ExtractString(),
      "webauthn: OK");

  static constexpr char kMakeCredentialCrossDomainNoHostPerms[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { id: "example.com", name: "" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
  }}).then(c => 'webauthn: OK',
           e => 'error ' + e);
})())";

  // This should fail with INVALID_PROTOCOL and never one of the errors from
  // related-origin processing because extensions don't participate in that
  // system.
  EXPECT_THAT(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      kMakeCredentialCrossDomainNoHostPerms)
          .ExtractString(),
      testing::HasSubstr("Public-key credentials are only available to"));
}

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest, ReportInvalidStrings) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  // This should fail with a TypeError due to an invalid base64url
  // string in the userId.
  EXPECT_THAT(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
  PublicKeyCredential.signalAllAcceptedCredentials({
      rpId: "www.example.com",
      userId: "a/+c+/c",
      allAcceptedCredentialIds: ["AQIDBAUGBwgJCgsMDQ4PEA"],
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )")
          .ExtractString(),
      testing::HasSubstr("Invalid base64url string for userId."));

  // This should fail with a TypeError due to an invalid base64url
  // string in the allAcceptedCredentialIds list.
  EXPECT_THAT(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
  PublicKeyCredential.signalAllAcceptedCredentials({
    rpId: "www.example.com",
    userId: "BQYHCA",
    allAcceptedCredentialIds: ["/a+/b+/c"],
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )")
          .ExtractString(),
      testing::HasSubstr(
          "Invalid base64url string for allAcceptedCredentialIds."));

  // This should fail with a TypeError due to an invalid base64url
  // string in the userId of signalCurrentUserDetails;
  EXPECT_THAT(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      R"(
  PublicKeyCredential.signalCurrentUserDetails({
    rpId: "www.example.com",
    userId: "A+/+A",
    name: "Pepito",
    displayName: "Pepito The Cat",
  }).then(c => 'webauthn: OK', e => 'error ' + e);
  )")
          .ExtractString(),
      testing::HasSubstr("Invalid base64url string for userId."));

  // This should fail with a TypeError due to an invalid base64url
  // string in the credentialId report.
  EXPECT_THAT(
      content::EvalJs(
          browser()->tab_strip_model()->GetActiveWebContents(),
          base::ReplaceStringPlaceholders(kSignalUnknownCredentialId, {"A+/+A"},
                                          /*offsets=*/nullptr))
          .ExtractString(),
      testing::HasSubstr("Invalid base64url string for credentialId."));
}

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest, ConditionalMediationUnavailable) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(false, content::EvalJs(
                       web_contents,
                       "PublicKeyCredential.isConditionalMediationAvailable()"));
  EXPECT_EQ(false, content::EvalJs(web_contents, R"(
  PublicKeyCredential.getClientCapabilities().then(c => c.conditionalGet);
  )"));
}

class WebAuthnHintsTest : public WebAuthnBrowserTest {
  class Observer : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    virtual ~Observer() = default;

    void WaitForHints() { run_loop_.Run(); }

    const content::AuthenticatorRequestClientDelegate::Hints& hints() const {
      return hints_;
    }

    void HintsSet(const content::AuthenticatorRequestClientDelegate::Hints&
                      hints) override {
      hints_ = hints;
      run_loop_.Quit();
    }

   private:
    content::AuthenticatorRequestClientDelegate::Hints hints_;
    base::RunLoop run_loop_;
  };

  void SetUpOnMainThread() override {
    WebAuthnBrowserTest::SetUpOnMainThread();
    observer_ = std::make_unique<Observer>();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("www.example.com", "/title1.html")));

    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory_ = virtual_device_factory.get();
    auth_env_ =
        std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
            std::move(virtual_device_factory));

    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
        observer_.get());
  }

  void PostRunTestOnMainThread() override {
    // To avoid dangling raw_ptr's these values need to be destroyed before
    // this test class.
    virtual_device_factory_ = nullptr;
    auth_env_.reset();
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    WebAuthnBrowserTest::PostRunTestOnMainThread();
  }

 protected:
  std::unique_ptr<Observer> observer_;
  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_;
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting> auth_env_;
};

IN_PROC_BROWSER_TEST_F(WebAuthnHintsTest, HintsArePassedThrough) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialWithHints);
  observer_->WaitForHints();

  ASSERT_TRUE(observer_->hints().transport.has_value());
  EXPECT_EQ(observer_->hints().transport.value(),
            AuthenticatorTransport::kHybrid);
}

class WebAuthnConditionalUITest : public WebAuthnBrowserTest {
  class Observer : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    enum State {
      kHasNotShowedUI,
      kWaitingForUI,
      kShowedUI,
    };
    virtual ~Observer() = default;
    void WaitForUI() {
      if (state_ != kHasNotShowedUI) {
        return;
      }
      state_ = kWaitingForUI;
      run_loop_.Run();
    }

    // ChromeAuthenticatorRequestDelegate::TestObserver:
    void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai)
        override {}

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
      if (state_ == kWaitingForUI) {
        // When the content layer controls authenticator dispatch, dispatching
        // happens on tasks posted right before the UI is shown. We need to
        // QuitWhenIdle to make sure that, if an authenticator is dispatched to,
        // that task has a chance to finish before the test continues. That way
        // we can catch any potentially unexpected authenticator dispatches.
        run_loop_.QuitWhenIdle();
      }
      state_ = kShowedUI;
    }

    void AccountSelectorShown(
        const std::vector<device::AuthenticatorGetAssertionResponse>& responses)
        override {
      for (const auto& response : responses) {
        accounts_.emplace_back(base::HexEncode(response.credential->id));
      }
    }

    std::vector<std::string> accounts_;

   private:
    State state_ = kHasNotShowedUI;
    base::RunLoop run_loop_;
  };

  void SetUpOnMainThread() override {
    WebAuthnBrowserTest::SetUpOnMainThread();
    observer_ = std::make_unique<Observer>();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("www.example.com", "/title1.html")));

    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory_ = virtual_device_factory.get();
    virtual_device_factory->mutable_state()->InjectResidentKey(
        kCredentialID, "www.example.com", std::vector<uint8_t>{5, 6, 7, 8},
        "flandre", "Flandre Scarlet");
    virtual_device_factory->mutable_state()->fingerprints_enrolled = true;
    device::VirtualCtap2Device::Config config;
    config.resident_key_support = true;
    config.internal_uv_support = true;
    virtual_device_factory->SetCtap2Config(std::move(config));
    auth_env_ =
        std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
            std::move(virtual_device_factory));

    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
        observer_.get());
  }

  void PostRunTestOnMainThread() override {
    // To avoid dangling raw_ptr's these values need to be destroyed before
    // this test class.
    virtual_device_factory_ = nullptr;
    auth_env_.reset();
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    WebAuthnBrowserTest::PostRunTestOnMainThread();
  }

 protected:
  std::unique_ptr<Observer> observer_;
  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_;
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting> auth_env_;
};

static constexpr char kConditionalUIRequest[] = R"((() => {
window.requestAbortController = new AbortController();
navigator.credentials.get({
  signal: window.requestAbortController.signal,
  mediation: 'conditional',
  publicKey: {
    challenge: new Uint8Array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]),
    timeout: 10000,
    allowCredentials: [],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

// Tests that the "Sign in with another device…" button dispatches requests to
// plugged in authenticators.
IN_PROC_BROWSER_TEST_F(WebAuthnConditionalUITest,
                       ConditionalUIOtherDeviceButton) {
  // Make a Conditional UI request. The authenticator should not be dispatched
  // to before the user clicks the "Sign in with another device…" button.
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([](device::VirtualFidoDevice* device) -> bool {
        NOTREACHED() << "Virtual device should not have been dispatched to";
      });
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kConditionalUIRequest);
  observer_->WaitForUI();

  // Allow the virtual device to respond to requests, then simulate clicking the
  // "Sign in with another device…" button and wait for a result.
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting(
          [&](device::VirtualFidoDevice* device) { return true; });
  ChromeWebAuthnCredentialsDelegate delegate(
      web_contents->GetPrimaryMainFrame());
  delegate.LaunchSecurityKeyOrHybridFlow();

  std::string result;
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_EQ(result, "\"webauthn: OK\"");
  EXPECT_EQ(observer_->accounts_.size(), 1u);
  EXPECT_EQ(observer_->accounts_.at(0), "0102030405060708090A0B0C0D0E0F10");
}

class WebAuthnImmediateGetTest : public WebAuthnBrowserTest {
 protected:
  static constexpr std::string_view kRequestWithPasswordTemplate = R"(
    navigator.credentials.get({
    uiMode: 'immediate',
    password: $1,
    publicKey: {
      challenge: new Uint8Array([1,3,2,7,1,3,2,7]),
      timeout: 10000,
      userVerification: 'discouraged',
    }}).then(c => 'webauthn: OK', e => 'error ' + e);
  )";

  static constexpr std::string_view kRequestWithAllowlistTemplate = R"(
    navigator.credentials.get({
    uiMode: 'immediate',
    publicKey: {
      challenge: new Uint8Array([1,3,2,7,1,3,2,7]),
      allowCredentials: [$1],
      timeout: 10000,
      userVerification: 'discouraged',
    }}).then(c => 'webauthn: OK', e => 'error ' + e);
  )";

};

IN_PROC_BROWSER_TEST_F(WebAuthnImmediateGetTest, NoCreds_NotFoundError) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  for (const auto& request_password : {"false", "true"}) {
    const auto& script = base::ReplaceStringPlaceholders(
        kRequestWithPasswordTemplate, {request_password},
        /*offsets=*/nullptr);
    const auto& result = content::EvalJs(web_contents, script);
    EXPECT_THAT(result.ExtractString(), testing::HasSubstr("NotAllowedError"));
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthnImmediateGetTest,
                       Incognito_NoCreds_NotFoundError) {
  auto* otr_browser = OpenURLOffTheRecord(
      browser()->GetProfile(),
      https_server_.GetURL("www.example.com", "/title1.html"));
  content::WebContents* web_contents =
      otr_browser->tab_strip_model()->GetActiveWebContents();

  for (const auto& request_password : {"false", "true"}) {
    const auto& script = base::ReplaceStringPlaceholders(
        kRequestWithPasswordTemplate, {request_password},
        /*offsets=*/nullptr);
    const auto& result = content::EvalJs(web_contents, script);
    EXPECT_THAT(result.ExtractString(), testing::HasSubstr("NotAllowedError"));
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthnImmediateGetTest, Allowlist_NotAllowedError) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const auto& script = base::ReplaceStringPlaceholders(
      kRequestWithAllowlistTemplate,
      {"{type: 'public-key', id: new Uint8Array([1,3,2,7])}"},
      /*offsets=*/nullptr);
  const auto& result = content::EvalJs(web_contents, script);
  EXPECT_THAT(result.ExtractString(), testing::HasSubstr("NotAllowedError"));
}

// Reproduction test for Use-After-Free in AuthenticatorRequestDialogController
// (crbug.com/522566295).
class WebAuthnUAFReproductionTest
    : public WebAuthnBrowserTest,
      public ChromeAuthenticatorRequestDelegate::TestObserver,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  WebAuthnUAFReproductionTest() {
  }
  ~WebAuthnUAFReproductionTest() override = default;

  // ChromeAuthenticatorRequestDelegate::TestObserver:
  void Created(ChromeAuthenticatorRequestDelegate* delegate) override {
    delegate_ = delegate;
    model_ = delegate_->dialog_model();
    model_->AddObserver(this);
  }

  void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
    delegate_shown_future_.SetValue(delegate);
  }

  void OnDestroy(ChromeAuthenticatorRequestDelegate* delegate) override {
    delegate_ = nullptr;
    delegate_shown_future_.Clear();
  }

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override {
    if (model_ &&
        (model_->step() == AuthenticatorRequestDialogModel::Step::kClosed ||
         model_->step() ==
             AuthenticatorRequestDialogModel::Step::kPlatformAuthenticator)) {
      DeleteWebContents();
    }
  }

  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
    if (model_ == model) {
      model_ = nullptr;
    }
  }

  void DeleteWebContents() {
    if (web_contents_deleted_) {
      return;
    }
    web_contents_deleted_ = true;
    auto* tab_strip_model = browser()->tab_strip_model();
    int active_index = tab_strip_model->active_index();
    tab_strip_model->DetachAndDeleteWebContentsAt(active_index);
  }

  void SetUpOnMainThread() override {
    WebAuthnBrowserTest::SetUpOnMainThread();
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(this);
  }

  void TearDownOnMainThread() override {
    if (model_) {
      model_->RemoveObserver(this);
    }
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    WebAuthnBrowserTest::TearDownOnMainThread();
  }

  raw_ptr<ChromeAuthenticatorRequestDelegate, DisableDanglingPtrDetection>
      delegate_ = nullptr;
  raw_ptr<AuthenticatorRequestDialogModel, DisableDanglingPtrDetection> model_ =
      nullptr;
  base::test::TestFuture<
      raw_ptr<ChromeAuthenticatorRequestDelegate, DisableDanglingPtrDetection>>
      delegate_shown_future_;
  bool web_contents_deleted_ = false;
};

IN_PROC_BROWSER_TEST_F(WebAuthnUAFReproductionTest, CancelUAF) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Trigger WebAuthn flow asynchronously
  content::ExecuteScriptAsync(web_contents, R"(
    navigator.credentials.create({
      publicKey: {
        challenge: new Uint8Array([1, 2, 3, 4]),
        rp: { name: "Example" },
        user: {
          id: new Uint8Array([1, 2, 3, 4]),
          name: "test",
          displayName: "test"
        },
        pubKeyCredParams: [{ type: "public-key", alg: -7 }],
        timeout: 60000
      }
    });
  )");

  ASSERT_TRUE(delegate_shown_future_.Wait());
  ASSERT_TRUE(delegate_);
  ASSERT_TRUE(delegate_->dialog_controller());

  // Force controller step to an error state where is_request_complete() is true
  delegate_->dialog_controller()->SetCurrentStepForTesting(
      AuthenticatorRequestDialogModel::Step::kKeyNotRegistered);

  // Trigger UAF
  delegate_->dialog_controller()->CancelAuthenticatorRequest();
}

IN_PROC_BROWSER_TEST_F(WebAuthnUAFReproductionTest, HideDialogUAF) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Trigger WebAuthn flow asynchronously
  content::ExecuteScriptAsync(web_contents, R"(
    navigator.credentials.create({
      publicKey: {
        challenge: new Uint8Array([1, 2, 3, 4]),
        rp: { name: "Example" },
        user: {
          id: new Uint8Array([1, 2, 3, 4]),
          name: "test",
          displayName: "test"
        },
        pubKeyCredParams: [{ type: "public-key", alg: -7 }],
        timeout: 60000
      }
    });
  )");

  ASSERT_TRUE(delegate_shown_future_.Wait());
  ASSERT_TRUE(delegate_);
  ASSERT_TRUE(delegate_->dialog_controller());

  // Trigger UAF
  delegate_->dialog_controller()->HideDialogAndDispatchToPlatformAuthenticator(
      std::nullopt);
}

}  // namespace
