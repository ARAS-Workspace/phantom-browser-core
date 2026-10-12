// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_web_authentication_delegate.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/public/features.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/test/base/testing_profile.h"
#include "device/fido/mac/authenticator_config.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

using TransportAvailabilityInfo =
    device::FidoRequestHandlerBase::TransportAvailabilityInfo;

class Observer : public testing::NiceMock<
                     ChromeAuthenticatorRequestDelegate::TestObserver> {
 public:
  MOCK_METHOD(void,
              Created,
              (ChromeAuthenticatorRequestDelegate * delegate),
              (override));
  MOCK_METHOD(void,
              OnTransportAvailabilityEnumerated,
              (ChromeAuthenticatorRequestDelegate * delegate,
               TransportAvailabilityInfo* tai),
              (override));
  MOCK_METHOD(void,
              UIShown,
              (ChromeAuthenticatorRequestDelegate * delegate),
              (override));
};

class ChromeWebAuthenticationDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(&observer_);
  }

  void TearDown() override {
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  Observer observer_;
};

TEST_F(ChromeWebAuthenticationDelegateTest, IndividualAttestation) {
  static const struct TestCase {
    std::string name;
    std::string origin;
    std::string rp_id;
    std::string enterprise_attestation_switch_value;
    std::vector<std::string> permit_attestation_policy_values;
    bool expected;
  } kTestCases[] = {
      {"Basic", "https://login.example.com", "example.com", "", {}, false},
      {"Policy permits RP ID",
       "https://login.example.com",
       "example.com",
       "",
       {"example.com", "other.com"},
       true},
      {"Policy doesn't permit RP ID",
       "https://login.example.com",
       "example.com",
       "",
       {"other.com", "login.example.com", "https://example.com",
        "http://example.com", "https://login.example.com", "com", "*"},
       false},
      {"Policy doesn't care about the origin",
       "https://login.example.com",
       "example.com",
       "",
       {"https://login.example.com", "https://example.com"},
       false},
      {"Switch permits origin",
       "https://login.example.com",
       "example.com",
       "https://login.example.com,https://other.com,xyz:/invalidorigin",
       {},
       true},
      {"Switch doesn't permit origin",
       "https://login.example.com",
       "example.com",
       "example.com,login.example.com,http://login.example.com,https://"
       "example.com,https://a.login.example.com,https://*.example.com",
       {},
       false},
  };
  for (const auto& test : kTestCases) {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        webauthn::switches::kPermitEnterpriseAttestationOriginList,
        test.enterprise_attestation_switch_value);
    PrefService* prefs =
        Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
    if (!test.permit_attestation_policy_values.empty()) {
      base::ListValue policy_values;
      for (const std::string& v : test.permit_attestation_policy_values) {
        policy_values.Append(v);
      }
      prefs->SetList(prefs::kSecurityKeyPermitAttestation,
                     std::move(policy_values));
    } else {
      prefs->ClearPref(prefs::kSecurityKeyPermitAttestation);
    }
    ChromeWebAuthenticationDelegate delegate;
    EXPECT_EQ(delegate.ShouldPermitIndividualAttestation(
                  GetBrowserContext(), url::Origin::Create(GURL(test.origin)),
                  test.rp_id),
              test.expected)
        << test.name;
  }
}

constexpr char kExtensionId[] = "extension-id";
constexpr char kExtensionOrigin[] = "chrome-extension://extension-id";

typedef struct {
  const char* pattern;
  const char* rp_id;
} PatternRpIdPair;

constexpr PatternRpIdPair kValidRelyingPartyTestCases[] = {
    // Extensions are always allowed to claim their own origins.
    {"", kExtensionId},

    {"<all_urls>", "google.com"},
    {"https://*/*", "google.com"},
    {"https://google.com/", "google.com"},
    {"https://*.google.com/", "google.com"},

    // Localhost is special cased.
    {"http://localhost/", "localhost"},

    // Sanity check empty domain parts.
    // URLPattern trims all trailing dots from hosts for matching (both from the
    // pattern and from the evaluated host). Thus, patterns or hosts with any
    // number of trailing dots are canonicalized to the version without trailing
    // dots and match.
    {"https://google.com./", "google.com"},
    {"https://google.com./", "google.com."},
    {"https://google.com/", "google.com."},
    {"https://google.com/", "google.com.."},
    {"https://google.com../", "google.com"},
};

constexpr PatternRpIdPair kInvalidRelyingPartyTestCases[] = {
    // Extensions are not allowed to claim RP IDs belonging to other extensions.
    {"chrome-extension://some-other-extension/",
     "chrome-extension://some-other-extension/"},
    {"chrome-extension://some-other-extension/", "some-other-extension"},

    // Extensions are not allowed to claim RP IDs matching eTLDs, even if they
    // have host permissions over their origins.
    {"<all_urls>", "com"},
    {"https://*/*", "com"},
    {"https://com/", "com"},

    // Extensions that have access to non default ports are not allowed to claim
    // RP IDs matching their origin.
    {"https://google.com:1337/", "google.com"},

    // Unlike regular WebAuthn processing rules, extensions are not allowed to
    // claim higher level domains if they have permissions over a subdomain.
    {"https://*.subdomain.google.com/", "google.com"},

    // Single component domains are considered eTLDs, even if not on the PSL.
    {"https://myawesomedomain/", "myawesomedomain"},

    // The rules below are a sanity check to verify that the implementation
    // matches webauthn rules and are copied from
    // content/browser/webauth/authenticator_impl_unittest.cc.
    {"https://google.com/", "com"},
    {"http://google.com/", "google.com"},
    {"http://myawesomedomain/", "myawesomedomain"},
    {"https://google.com/", "foo.bar.google.com"},
    {"http://myawesomedomain/", "randomdomain"},
    {"https://myawesomedomain/", "randomdomain"},
    {"https://notgoogle.com/", "google.com)"},
    {"https://not-google.com/", "google.com)"},
    {"https://evil.appspot.com/", "appspot.com"},
    {"https://evil.co.uk/", "co.uk"},
    {"https://google.com/", ".google.com"},
    {"https://.com/", "com."},
    {"https://.co.uk/", "co.uk."},
    {"https://1.2.3/", "1.2.3"},
    {"https://1.2.3/", "2.3"},
    {"https://127.0.0.1/", "127.0.0.1"},
    {"https://127.0.0.1/", "27.0.0.1"},
    {"https://127.0.0.1/", ".0.0.1"},
    {"https://127.0.0.1/", "0.0.1"},
    {"https://[::127.0.0.1]/", "127.0.0.1"},
    {"https://[::127.0.0.1]/", "[127.0.0.1]"},
    {"https://[::1]/", "1"},
    {"https://[::1]/", "1]"},
    {"https://[::1]/", "::1"},
    {"https://[::1]/", "[::1]"},
    {"https://[1::1]/", "::1"},
    {"https://[1::1]/", "::1]"},
    {"https://[1::1]/", "[::1]"},
    {"http://google.com:443/", "google.com"},
    {"data:google.com/", "google.com"},
    {"data:text/html,google.com/", "google.com"},
    {"ws://google.com/", "google.com"},
    {"ftp://google.com/", "google.com"},
    {"file://google.com/", "google.com"},
    {"wss://google.com/", "google.com"},
    {"data:,/", ""},
    {"https://google.com/", ""},
    {"ws://google.com/", ""},
    {"wss://google.com/", ""},
    {"ftp://google.com/", ""},
    {"file://google.com/", ""},
    {"https://login.awesomecompany/", "awesomecompany"},
};

// Tests that an extension origin can claim relying party IDs it has permissions
// for.
TEST_F(ChromeWebAuthenticationDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest_ExtensionValidCases) {
  for (const auto& test : kValidRelyingPartyTestCases) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("Extension name")
            .SetID(kExtensionId)
            .AddHostPermission(test.pattern)
            .Build();
    extensions::ExtensionRegistry::Get(browser_context())
        ->AddEnabled(extension);

    ChromeWebAuthenticationDelegate delegate;
    SCOPED_TRACE(::testing::Message() << "rp_id=" << test.rp_id);
    SCOPED_TRACE(::testing::Message() << "pattern=" << test.pattern);
    EXPECT_TRUE(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
        GetBrowserContext(), url::Origin::Create(GURL(kExtensionOrigin)),
        test.rp_id));
  }
}

// Tests that an extension origin cannot claim relying party IDs it does not
// have permissions for.
TEST_F(ChromeWebAuthenticationDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest_ExtensionInvalidCases) {
  for (const auto& test : kInvalidRelyingPartyTestCases) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("Extension name")
            .SetID(kExtensionId)
            .AddHostPermission(test.pattern)
            .Build();
    extensions::ExtensionRegistry::Get(browser_context())
        ->AddEnabled(extension);

    ChromeWebAuthenticationDelegate delegate;
    SCOPED_TRACE(::testing::Message() << "rp_id=" << test.rp_id);
    SCOPED_TRACE(::testing::Message() << "pattern=" << test.pattern);
    EXPECT_FALSE(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
        GetBrowserContext(), url::Origin::Create(GURL(kExtensionOrigin)),
        test.rp_id));
  }
}

// Tests that OverrideCallerOriginAndRelyingPartyIdValidation returns false for
// chrome-extension origins that don't match an active extension.
TEST_F(ChromeWebAuthenticationDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest_ExtensionNotFound) {
  ChromeWebAuthenticationDelegate delegate;
  EXPECT_FALSE(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
      GetBrowserContext(), url::Origin::Create(GURL(kExtensionOrigin)),
      kExtensionId));
}

// Tests that OverrideCallerOriginAndRelyingPartyIdValidation returns false for
// chrome-extension origins that are restricted by policy.
// Regression test for https://crbug.com/494341321.
TEST_F(ChromeWebAuthenticationDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest_PolicyRestriction) {
  // Block extensions from google.com through policy.
  int context_id = extensions::util::GetBrowserContextId(browser_context());
  URLPattern default_policy_blocked_pattern =
      URLPattern(URLPattern::SCHEME_ALL, "*://google.com/*");
  extensions::URLPatternSet default_allowed_hosts;
  extensions::URLPatternSet default_blocked_hosts;
  default_blocked_hosts.AddPattern(default_policy_blocked_pattern);
  extensions::PermissionsData::SetDefaultPolicyHostRestrictions(
      context_id, default_blocked_hosts, default_allowed_hosts);

  // Create an extension that has permissions over every site.
  ChromeWebAuthenticationDelegate delegate;
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Extension name")
          .SetID(kExtensionId)
          .AddHostPermission("<all_urls>")
          .Build();
  // Extensions need a context ID set to respect policy.
  extension->permissions_data()->SetContextId(context_id);
  extensions::ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  // Verify the extension is not allowed to claim google.com.
  EXPECT_FALSE(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
      GetBrowserContext(), url::Origin::Create(GURL(kExtensionOrigin)),
      "google.com"));
}

// Tests that OverrideCallerOriginAndRelyingPartyIdValidation returns false for
// web origins.
TEST_F(ChromeWebAuthenticationDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest_WebOrigin) {
  ChromeWebAuthenticationDelegate delegate;
  EXPECT_FALSE(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
      GetBrowserContext(), url::Origin::Create(GURL("https://google.com")),
      kExtensionId));
}

TEST_F(ChromeWebAuthenticationDelegateTest, MaybeGetRelyingPartyIdOverride) {
  ChromeWebAuthenticationDelegate delegate;
  static const struct {
    std::string rp_id;
    std::string origin;
    std::optional<std::string> expected;
  } kTests[] = {
      {"example.com", "https://example.com", std::nullopt},
      {"foo.com", "https://example.com", std::nullopt},
      {"example.com", kExtensionOrigin, std::nullopt},
      {kExtensionId, kExtensionOrigin, kExtensionOrigin},
  };
  for (const auto& test : kTests) {
    EXPECT_EQ(delegate.MaybeGetRelyingPartyIdOverride(
                  test.rp_id, url::Origin::Create(GURL(test.origin))),
              test.expected);
  }
}

}  // namespace
