// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_web_authentication_delegate_base.h"

#include <algorithm>
#include <cstddef>
#include <string_view>

#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "device/fido/public/features.h"

namespace {

class OriginMayUseRemoteDesktopClientOverrideTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  static constexpr char kCorpCrdOrigin[] =
      "https://remotedesktop.corp.google.com";
  static constexpr char kCorpCrdAutopushOrigin[] =
      "https://remotedesktop-autopush.corp.google.com/";
  static constexpr char kCorpCrdDailyOrigin[] =
      "https://remotedesktop-daily-6.corp.google.com/";

  const std::array<const char*, 3> kCorpCrdOrigins = {
      kCorpCrdOrigin, kCorpCrdAutopushOrigin, kCorpCrdDailyOrigin};

  static constexpr char kExampleOrigin[] = "https://example.com";
  static constexpr char kAnotherExampleOrigin[] = "https://another.example.com";
#if !BUILDFLAG(IS_ANDROID)
  static constexpr char kTestIsolatedAppOrigin[] =
      "isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
#endif
  static constexpr std::string_view kTestAtExampleDotCom = "test@example.com";

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetupUserAffiliation(true, kTestAtExampleDotCom);
  }

  void SetupUserAffiliation(bool is_affiliated, std::string_view email) {
    const base::flat_set<std::string> affiliation_ids =
        is_affiliated ? base::flat_set<std::string>({"test-affiliation-id"})
                      : base::flat_set<std::string>();

    g_browser_process->browser_policy_connector()
        ->SetDeviceAffiliatedIdsForTesting(affiliation_ids);

    profile()->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting(
        affiliation_ids);
  }

  void LogOutUser(std::string_view email) {
    g_browser_process->browser_policy_connector()
        ->SetDeviceAffiliatedIdsForTesting({});
    profile()->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting({});
  }
};

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AdditionalOriginSwitch_WithAllowedOriginsPolicy) {
  // The --webauthn-remote-proxied-requests-allowed-additional-origin switch
  // allows passing an additional origin for testing. This origin will be
  // allowed if the WebAuthenticationRemoteDesktopAllowedOrigins policy is set
  // to a non-empty list of origins.  If the policy is set, the command-line
  // origin is treated as another allowed origin in addition to those specified
  // by the policy.
  ChromeWebAuthenticationDelegateBase delegate;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,
      kExampleOrigin);

  // Initially, no origins should be allowed because the allowed origins pref
  // hasn't been set yet.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kAnotherExampleOrigin))));

  // Set the allowed origins pref to include another origin.
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append(kAnotherExampleOrigin));

  // Both the origin specified by the command-line switch and the origin in the
  // allowed origins pref should be allowed.
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kAnotherExampleOrigin))));

  // Google Corp CRD origins are not affected by either the switch or this
  // policy.
  for (auto* origin : kCorpCrdOrigins) {
    EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
        browser_context(), url::Origin::Create(GURL(origin))));
  }

  // Origins not listed in either the switch or the policy remain disallowed.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://very.other.example.com"))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AdditionalOriginSwitch_WithAllowedOriginsPolicy_NoAffiliation) {
  // Version of a test case above but for non-affiliated user
  ChromeWebAuthenticationDelegateBase delegate;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,
      kExampleOrigin);

  // Log user out, no affiliated user exists for this test
  LogOutUser(kTestAtExampleDotCom);

  // Set the allowed origins pref to include another origin.
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append(kAnotherExampleOrigin));

  // False when user is not affiliated or no user regardless if policy set with
  // origin
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kAnotherExampleOrigin))));

  // Behavior for case without affiliated user in a sense of corporate origins
  // should not change
  for (auto* origin : kCorpCrdOrigins) {
    EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
        browser_context(), url::Origin::Create(GURL(origin))));
  }

  // Origins not listed in either the switch or the policy remain disallowed.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://very.other.example.com"))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AdditionalOriginSwitch_WithExplicitlyEmptyAllowedOriginsPolicy) {
  // The --webauthn-remote-proxied-requests-allowed-additional-origin switch
  // should be ignored when the allowed origins policy list is empty.
  ChromeWebAuthenticationDelegateBase delegate;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,
      kExampleOrigin);

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  // Test with policy unset.
  prefs->ClearPref(webauthn::pref_names::kRemoteDesktopAllowedOrigins);
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));

  // Test with policy explicitly empty.
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue());
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
}

#if !BUILDFLAG(IS_ANDROID)  // IWAs aren't supported on Android
TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_IWAAccepted) {
  ChromeWebAuthenticationDelegateBase delegate;
  base::test::ScopedFeatureList scoped_feature_list(
      device::kWebAuthnIWARemoteDesktopAllowedOriginsPolicy);

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append(kTestIsolatedAppOrigin));

  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kTestIsolatedAppOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_IWANotAccepted_Another_Caller_Origin) {
  ChromeWebAuthenticationDelegateBase delegate;
  base::test::ScopedFeatureList scoped_feature_list(
      device::kWebAuthnIWARemoteDesktopAllowedOriginsPolicy);

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append(kExampleOrigin));

  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kTestIsolatedAppOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_IWAsNotAccepted_Feature_Off) {
  ChromeWebAuthenticationDelegateBase delegate;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      device::kWebAuthnIWARemoteDesktopAllowedOriginsPolicy);

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append(kTestIsolatedAppOrigin));

  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kTestIsolatedAppOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_IWAsNotAccepted_Origin_Not_Listed) {
  ChromeWebAuthenticationDelegateBase delegate;
  base::test::ScopedFeatureList scoped_feature_list(
      device::kWebAuthnIWARemoteDesktopAllowedOriginsPolicy);

  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kTestIsolatedAppOrigin))));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_InvalidURLs) {
  ChromeWebAuthenticationDelegateBase delegate;

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  const std::vector<std::string> invalid_origins = {
      "invalid",
      "http://",
      "example.com",  // Missing scheme
      "https://example.com:invalidport",
  };

  base::ListValue invalid_origins_list;
  for (const auto& origin : invalid_origins) {
    invalid_origins_list.Append(origin);
  }
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 std::move(invalid_origins_list));

  // None of the above invalid origins should grant access.
  for (const auto& origin : invalid_origins) {
    EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
        browser_context(), url::Origin::Create(GURL(origin))));
  }

  // A valid one, added for good measure, should still work.
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append(kExampleOrigin));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_MultipleValidURLs) {
  ChromeWebAuthenticationDelegateBase delegate;

  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  base::ListValue valid_origins;
  valid_origins.Append(kExampleOrigin);
  valid_origins.Append(kAnotherExampleOrigin);
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 std::move(valid_origins));

  // Both origins specified in the policy should grant access.
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kAnotherExampleOrigin))));

  // An unrelated origin should not be allowed.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://very.other.example.com"))));
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       AllowedOriginsPolicy_SchemePortPathMismatch) {
  ChromeWebAuthenticationDelegateBase delegate;
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();

  // Scheme mismatch.
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append("https://example.com"));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL("http://example.com"))));

  // Port mismatch.
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append("https://example.com:1234"));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL("https://example.com"))));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://example.com:5678"))));

  // Path mismatch (should be allowed because paths are ignored).
  prefs->SetList(webauthn::pref_names::kRemoteDesktopAllowedOrigins,
                 base::ListValue().Append("https://example.com/path"));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL("https://example.com"))));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://example.com/otherpath"))));
}

}  // namespace
