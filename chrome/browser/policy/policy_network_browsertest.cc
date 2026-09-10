// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span_reader.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/heap_profiling/in_process/heap_profiler_parameters.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/util.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_doh_server.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/boringssl/src/include/openssl/tls1.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
// Used by DisableWithNewProfile test. See the comment in above that test.
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace policy {

namespace {

// Returns an SSLServerConfig that causes the test server to reject any TLS
// handshake unless it specifies the three TLS 1.3 cipher names in the expected
// order for CNSA, which is different from the default order. This effectively
// causes the connection to fail unless the PreferSlowCiphers policy is
// correctly configured for "cnsa".
net::SSLServerConfig GetServerConfigForPreferSlowCiphersTest() {
  const auto kExpectedCipherNamesWithPolicy = std::to_array<const char*>(
      {"TLS_AES_256_GCM_SHA384", "TLS_AES_128_GCM_SHA256",
       "TLS_CHACHA20_POLY1305_SHA256"});

  net::SSLServerConfig ssl_config;
  ssl_config.version_min = net::SSL_PROTOCOL_VERSION_TLS1_3;
  ssl_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1_3;
  // Make the test server only accept the expected TLS 1.3 ciphers in the
  // order specified by the policy, or otherwise reject the handshake.
  ssl_config.client_hello_callback_for_testing =
      base::BindLambdaForTesting([&](const SSL_CLIENT_HELLO* client_hello) {
        // Each cipher is encoded as a two-byte, big-endian integer.
        CHECK(client_hello->cipher_suites_len % 2 == 0);
        // SAFETY: BoringSSL API guarantees that `client_hello->cipher_suites`
        // and `client_hello->ciphers_suites_len` describe a valid span.
        auto cipher_suites_reader = base::SpanReader{UNSAFE_BUFFERS(base::span{
            client_hello->cipher_suites, client_hello->cipher_suites_len})};
        uint16_t value;
        std::vector<std::string> cipher_names;
        while (cipher_suites_reader.ReadU16BigEndian(value)) {
          // Skip values we don't recognize as any TLS 1.3 cipher.
          const SSL_CIPHER* cipher = SSL_get_cipher_by_value(value);
          if (!cipher || SSL_CIPHER_get_min_version(cipher) != TLS1_3_VERSION) {
            continue;
          }
          cipher_names.emplace_back(SSL_CIPHER_standard_name(cipher));
        }
        return !std::ranges::search(cipher_names,
                                    kExpectedCipherNamesWithPolicy)
                    .empty();
      });
  return ssl_config;
}

bool GetLocalStateBooleanPref(const std::string& pref_name) {
  return g_browser_process->local_state()->GetBoolean(pref_name);
}

std::optional<std::string> GetManagedStringPref(
    PrefService* prefs,
    const std::string_view pref_name) {
  if (prefs->IsManagedPreference(pref_name)) {
    return prefs->GetString(pref_name);
  }
  return std::nullopt;
}

std::optional<std::string> GetLocalStateManagedStringPref(
    const std::string_view pref_name) {
  return GetManagedStringPref(g_browser_process->local_state(), pref_name);
}

}  // namespace

class SSLPolicyTest : public PolicyTest {
 public:
  SSLPolicyTest() = default;
  ~SSLPolicyTest() override = default;

  void TearDownOnMainThread() override {
    ASSERT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
    PolicyTest::TearDownOnMainThread();
  }

 protected:
  struct LoadResult {
    bool success;
    std::u16string title;
  };

  bool StartTestServer(const net::SSLServerConfig& ssl_config) {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    return https_server_.Start();
  }

  LoadResult LoadPage(std::string_view path) {
    return LoadPage(https_server_.GetURL(path));
  }

  LoadResult LoadPage(const GURL& url) {
    EXPECT_TRUE(NavigateToUrl(url, this));
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    if (web_contents->GetController().GetLastCommittedEntry()->GetPageType() ==
        content::PAGE_TYPE_ERROR) {
      return LoadResult{false, u""};
    }
    return LoadResult{true, web_contents->GetTitle()};
  }

  void ExpectVersionOrCipherMismatch() {
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    EXPECT_TRUE(content::EvalJs(web_contents,
                                "document.body.innerHTML.indexOf('ERR_SSL_"
                                "VERSION_OR_CIPHER_MISMATCH') >= 0")
                    .ExtractBool());
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(SSLPolicyTest, PreferSlowKexAlgorithmsPolicy) {
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_ML_KEM_1024};
  ASSERT_TRUE(StartTestServer(ssl_config));

  // Should fail to load a page from the test server because, by default, we
  // don't negotiate ML-KEM-1024.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            std::nullopt);
  LoadResult result = LoadPage("/title2.html");
  EXPECT_FALSE(result.success);

  // Set the policy to cnsa2 to prefer ML-KEM-1024.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowKexAlgorithms, base::Value("cnsa2"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should now succeed.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            "cnsa2");
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  // Set the policy to an unrecognized value; this falls back to the defaults.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowKexAlgorithms, base::Value("bogus"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should now fail.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            "bogus");
  result = LoadPage("/title2.html");
  EXPECT_FALSE(result.success);
}

IN_PROC_BROWSER_TEST_F(SSLPolicyTest, PreferSlowCiphersPolicy) {
  ASSERT_TRUE(StartTestServer(GetServerConfigForPreferSlowCiphersTest()));

  // Should fail to load a page from the test server because the default
  // cipher order doesn't match and the test server rejects the handshake.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers),
            std::nullopt);
  LoadResult result = LoadPage("/title2.html");
  EXPECT_FALSE(result.success);

  // Set the policy to cnsa to prefer the TLS 1.3 ciphers in the expected order.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowCiphers, base::Value("cnsa"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should now succeed.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers), "cnsa");
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  // Set the policy to an unrecognized value; this falls back to the defaults.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowCiphers, base::Value("bogus"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should now fail.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers), "bogus");
  result = LoadPage("/title2.html");
  EXPECT_FALSE(result.success);
}

// Tests the interaction between features::kCryptographyComplianceCnsa and the
// policies. The test parameter indicates whether the feature is enabled.
class SSLPolicyTestWithCnsaFeature : public SSLPolicyTest,
                                     public testing::WithParamInterface<bool> {
 public:
  SSLPolicyTestWithCnsaFeature() {
    features_.InitWithFeatureState(features::kCryptographyComplianceCnsa,
                                   IsCnsaFeatureEnabled());
  }

  bool IsCnsaFeatureEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         SSLPolicyTestWithCnsaFeature,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(SSLPolicyTestWithCnsaFeature,
                       CryptographyComplianceFeatureKeyExchange) {
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_ML_KEM_1024};
  ASSERT_TRUE(StartTestServer(ssl_config));

  // If the policies disable CNSA behavior, the base::Feature can override and
  // enable it.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowKexAlgorithms, base::Value("default"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            "default");
  LoadResult result = LoadPage("/title2.html");
  EXPECT_EQ(result.success, IsCnsaFeatureEnabled());

  // If the policy enables CNSA behavior, it is enabled regardless of the
  // base::Feature.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowKexAlgorithms, base::Value("cnsa2"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            "cnsa2");
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
}

IN_PROC_BROWSER_TEST_P(SSLPolicyTestWithCnsaFeature,
                       CryptographyComplianceFeatureCiphers) {
  ASSERT_TRUE(StartTestServer(GetServerConfigForPreferSlowCiphersTest()));

  // If the policy disables CNSA behavior, the base::Feature can override and
  // enable it.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowCiphers, base::Value("default"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers),
            "default");
  LoadResult result = LoadPage("/title2.html");
  EXPECT_EQ(result.success, IsCnsaFeatureEnabled());

  // If the policy enables CNSA behavior, it is enabled regardless of the
  // base::Feature.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowCiphers, base::Value("cnsa"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers), "cnsa");
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
}

class ECHPolicyTest : public SSLPolicyTest {
 public:
  // a.test is covered by `CERT_TEST_NAMES`.
  static constexpr std::string_view kHostname = "a.test";
  static constexpr std::string_view kPublicName = "public-name.test";
  static constexpr std::string_view kDohServerHostname = "doh.test";

  static constexpr std::string_view kECHSuccessTitle = "Negotiated ECH";
  static constexpr std::string_view kECHFailureTitle = "Did not negotiate ECH";

  ECHPolicyTest() : ech_server_{net::EmbeddedTestServer::TYPE_HTTPS} {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{net::features::kUseDnsHttpsSvcb,
          {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"}}}},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    // Configure `ech_server_` to enable and require ECH.
    net::SSLServerConfig server_config;
    std::vector<uint8_t> ech_config_list;
    server_config.ech_keys = net::MakeTestEchKeys(
        kPublicName, /*max_name_len=*/64, &ech_config_list);
    ASSERT_TRUE(server_config.ech_keys);
    ech_server_.RegisterRequestHandler(
        base::BindRepeating(&ECHPolicyTest::HandleRequest));
    ech_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES,
                             server_config);

    ASSERT_TRUE(ech_server_.Start());

    // Start a DoH server, which ensures we use a resolver with HTTPS RR
    // support. Configure it to serve records for `ech_server_`.
    doh_server_.SetHostname(kDohServerHostname);
    url::SchemeHostPort ech_host(GetURL("/"));
    doh_server_.AddAddressRecord(ech_host.host(),
                                 net::IPAddress::IPv4Localhost());
    doh_server_.AddRecord(net::BuildTestHttpsServiceRecord(
        net::dns_util::GetNameForHttpsQuery(ech_host),
        /*priority=*/1, /*service_name=*/ech_host.host(),
        {net::BuildTestHttpsServiceEchConfigParam(ech_config_list)}));
    ASSERT_TRUE(doh_server_.Start());

    // Add a single bootstrapping rule so we can resolve the DoH server.
    host_resolver()->AddRule(kDohServerHostname, "127.0.0.1");

    // The net stack doesn't enable DoH when it can't find a system DNS config
    // (see https://crbug.com/40198483).
    SetReplaceSystemDnsConfig();

    // Via policy, configure the network service to use `doh_server_`.
    UpdateProviderPolicy(PolicyMapWithDohServer());
    content::FlushNetworkServiceInstanceForTesting();
  }

  PolicyMap PolicyMapWithDohServer() {
    PolicyMap policies;
    SetPolicy(&policies, key::kDnsOverHttpsMode, base::Value("secure"));
    SetPolicy(&policies, key::kDnsOverHttpsTemplates,
              base::Value(doh_server_.GetTemplate()));
    return policies;
  }

  GURL GetURL(std::string_view path) {
    return ech_server_.GetURL(kHostname, path);
  }

 private:
  static std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type("text/html; charset=utf-8");
    if (request.ssl_info->encrypted_client_hello) {
      response->set_content(
          base::StrCat({"<title>", kECHSuccessTitle, "</title>"}));
    } else {
      response->set_content(
          base::StrCat({"<title>", kECHFailureTitle, "</title>"}));
    }
    return response;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::TestDohServer doh_server_;
  net::EmbeddedTestServer ech_server_;
};

IN_PROC_BROWSER_TEST_F(ECHPolicyTest, ECHEnabledPolicy) {
  // By default, the policy does not inhibit ECH.
  EXPECT_TRUE(GetLocalStateBooleanPref(prefs::kEncryptedClientHelloEnabled));
  LoadResult result = LoadPage(GetURL("/a"));
  EXPECT_TRUE(result.success);
  EXPECT_EQ(base::ASCIIToUTF16(kECHSuccessTitle), result.title);

  // Disable the policy.
  PolicyMap policies = PolicyMapWithDohServer();
  SetPolicy(&policies, key::kEncryptedClientHelloEnabled, base::Value(false));
  UpdateProviderPolicy(policies);
  content::FlushNetworkServiceInstanceForTesting();

  // ECH should no longer be enabled.
  EXPECT_FALSE(GetLocalStateBooleanPref(prefs::kEncryptedClientHelloEnabled));
  result = LoadPage(GetURL("/b"));
  EXPECT_TRUE(result.success);
  EXPECT_EQ(base::ASCIIToUTF16(kECHFailureTitle), result.title);
}

// TLS13EarlyDataPolicyTest relies on the fact that EmbeddedTestServer
// uses HTTP/1.1 without connection reuse (unless the protocol is explicitly
// specified). If EmbeddedTestServer ever gains connection reuse by default,
// we'll need to force it off.
class TLS13EarlyDataPolicyTestBase
    : public SSLPolicyTest,
      public ::testing::WithParamInterface<bool> {
 public:
  static constexpr std::string_view kHostname = "a.test";
  static constexpr std::string_view kEarlyDataCheckPath = "/test-request";
  static constexpr std::string_view kEarlyDataAcceptedTitle = "accepted";
  static constexpr std::string_view kEarlyDataNotAcceptedTitle = "not accepted";

  // net::features::kEnableTLS13EarlyData will be enabled or disabled depending
  // on the value of `feature_enabled`.
  explicit TLS13EarlyDataPolicyTestBase(bool feature_enabled)
      : test_server_{net::EmbeddedTestServer::TYPE_HTTPS} {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    auto& happy_eyeballs_feature_list =
        GetParam() ? enabled_features : disabled_features;
    happy_eyeballs_feature_list.emplace_back(net::features::kHappyEyeballsV3);
    auto& early_data_feature_list =
        feature_enabled ? enabled_features : disabled_features;
    early_data_feature_list.emplace_back(net::features::kEnableTLS13EarlyData);
    // Heap profiling makes the tests that call
    // content::RestartNetworkService() flakily crash. so force it off.
    // TODO(crbug.com/470131729): Re-enable heap profiler reporting.
    disabled_features.emplace_back(heap_profiling::kHeapProfilerReporting);

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
    // This is needed for the tests that restart the network service to work.
    content::ForceOutOfProcessNetworkService();
  }

  void SetUpOnMainThread() override {
    net::SSLServerConfig server_config;
    server_config.early_data_enabled = true;
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&TLS13EarlyDataPolicyTestBase::HandleRequest));
    test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES,
                              server_config);
    ASSERT_TRUE(test_server_.Start());

    host_resolver()->AddRule(kHostname, "127.0.0.1");
  }

  GURL GetURL(std::string_view path) {
    return test_server_.GetURL(kHostname, path);
  }

  GURL GetTestPageURL() { return GetURL("/index.html"); }

  void NavigateToTestPage() {
    ASSERT_TRUE(NavigateToUrl(GetTestPageURL(), this));
  }

  std::string FetchResourceForEarlyDataCheck(
      content::WebContents* for_web_contents = nullptr) {
    if (!for_web_contents) {
      for_web_contents = chrome_test_utils::GetActiveWebContents(this);
    }
    content::EvalJsResult result = content::EvalJs(
        for_web_contents, content::JsReplace(R"(
      fetch($1).then(res => res.text())
    )",
                                             GetURL(kEarlyDataCheckPath)));
    return result.ExtractString();
  }

  static void RestartNetworkServiceAndWaitUntilReady() {
    content::RestartNetworkService();
    content::FlushNetworkServiceInstanceForTesting();
  }

 private:
  static std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Connection", "close");
    if (request.GetURL().GetPath() == kEarlyDataCheckPath) {
      response->set_content_type("text/plain; charset=utf-8");
      if (request.ssl_info->early_data_accepted) {
        response->set_content(kEarlyDataAcceptedTitle);
      } else {
        response->set_content(kEarlyDataNotAcceptedTitle);
      }
    } else {
      response->set_content_type("text/html; charset=utf-8");
      response->set_content("<html></html>");
    }
    return response;
  }

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer test_server_;
};

class TLS13EarlyDataPolicyTest : public TLS13EarlyDataPolicyTestBase {
 public:
  TLS13EarlyDataPolicyTest()
      : TLS13EarlyDataPolicyTestBase(/*feature_enabled=*/false) {}
};

INSTANTIATE_TEST_SUITE_P(, TLS13EarlyDataPolicyTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(TLS13EarlyDataPolicyTest,
                       TLS13EarlyDataPolicyNoOverride) {
  EXPECT_FALSE(GetLocalStateBooleanPref(prefs::kTLS13EarlyDataEnabled));

  NavigateToTestPage();

  EXPECT_EQ(FetchResourceForEarlyDataCheck(), kEarlyDataNotAcceptedTitle);
}

// TODO(crbug.com/418717917, crbug.com/419211957): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TLS13EarlyDataPolicyEnable DISABLED_TLS13EarlyDataPolicyEnable
#else
#define MAYBE_TLS13EarlyDataPolicyEnable TLS13EarlyDataPolicyEnable
#endif
IN_PROC_BROWSER_TEST_P(TLS13EarlyDataPolicyTest,
                       MAYBE_TLS13EarlyDataPolicyEnable) {
  PolicyMap policies;
  SetPolicy(&policies, key::kTLS13EarlyDataEnabled, base::Value(true));
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(GetLocalStateBooleanPref(prefs::kTLS13EarlyDataEnabled));
  content::FlushNetworkServiceInstanceForTesting();

  NavigateToTestPage();

  EXPECT_EQ(FetchResourceForEarlyDataCheck(), kEarlyDataAcceptedTitle);
}

IN_PROC_BROWSER_TEST_P(TLS13EarlyDataPolicyTest, TLS13EarlyDataPolicyDisable) {
  PolicyMap policies;
  SetPolicy(&policies, key::kTLS13EarlyDataEnabled, base::Value(false));
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(GetLocalStateBooleanPref(prefs::kTLS13EarlyDataEnabled));

  NavigateToTestPage();

  EXPECT_EQ(FetchResourceForEarlyDataCheck(), kEarlyDataNotAcceptedTitle);
}

// TODO(crbug.com/475587477, crbug.com/477510552): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_EnableWithRestart DISABLED_EnableWithRestart
#else
#define MAYBE_EnableWithRestart EnableWithRestart
#endif
IN_PROC_BROWSER_TEST_P(TLS13EarlyDataPolicyTest, MAYBE_EnableWithRestart) {
  PolicyMap policies;
  SetPolicy(&policies, key::kTLS13EarlyDataEnabled, base::Value(true));
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(GetLocalStateBooleanPref(prefs::kTLS13EarlyDataEnabled));
  content::FlushNetworkServiceInstanceForTesting();

  // Restart to ensure that the policy setting persists.
  RestartNetworkServiceAndWaitUntilReady();

  NavigateToTestPage();

  EXPECT_EQ(FetchResourceForEarlyDataCheck(), kEarlyDataAcceptedTitle);
}

class TLS13EarlyDataPolicyEnabledByDefaultTest
    : public TLS13EarlyDataPolicyTestBase {
 public:
  TLS13EarlyDataPolicyEnabledByDefaultTest()
      : TLS13EarlyDataPolicyTestBase(/*feature_enabled=*/true) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         TLS13EarlyDataPolicyEnabledByDefaultTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(TLS13EarlyDataPolicyEnabledByDefaultTest, Disable) {
  PolicyMap policies;
  SetPolicy(&policies, key::kTLS13EarlyDataEnabled, base::Value(false));
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(GetLocalStateBooleanPref(prefs::kTLS13EarlyDataEnabled));
  content::FlushNetworkServiceInstanceForTesting();

  NavigateToTestPage();

  EXPECT_EQ(FetchResourceForEarlyDataCheck(), kEarlyDataNotAcceptedTitle);
}

IN_PROC_BROWSER_TEST_P(TLS13EarlyDataPolicyEnabledByDefaultTest,
                       DisableWithRestart) {
  PolicyMap policies;
  SetPolicy(&policies, key::kTLS13EarlyDataEnabled, base::Value(false));
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(GetLocalStateBooleanPref(prefs::kTLS13EarlyDataEnabled));
  content::FlushNetworkServiceInstanceForTesting();

  // Restart to ensure that the policy setting persists.
  RestartNetworkServiceAndWaitUntilReady();

  NavigateToTestPage();

  EXPECT_EQ(FetchResourceForEarlyDataCheck(), kEarlyDataNotAcceptedTitle);
}

#if !BUILDFLAG(IS_ANDROID)
// This test doesn't work on Android because off-the-record profiles work
// differently there. The behavior being tested is cross-platform, so it is not
// critical to test on Android, but it would be better.
// TODO(crbug.com/469517452): Make this test work on Android as well. Probably
// some extra abstractions should be added to policy_test_utils.h.
IN_PROC_BROWSER_TEST_P(TLS13EarlyDataPolicyEnabledByDefaultTest,
                       DisableWithNewOTPProfile) {
  PolicyMap policies;
  SetPolicy(&policies, key::kTLS13EarlyDataEnabled, base::Value(false));
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(GetLocalStateBooleanPref(prefs::kTLS13EarlyDataEnabled));
  content::FlushNetworkServiceInstanceForTesting();

  Browser* incognito_browser =
      CreateBrowserWindow(BrowserWindowCreateParams(
                              browser()->GetProfile()->GetPrimaryOTRProfile(
                                  /*create_if_needed=*/true),
                              /*from_user_gesture=*/true))
          ->GetBrowserForMigrationOnly();

  auto* render_frame_host = ui_test_utils::NavigateToURLWithDisposition(
      incognito_browser, GetTestPageURL(),
      WindowOpenDisposition::OFF_THE_RECORD,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(render_frame_host);

  EXPECT_EQ(FetchResourceForEarlyDataCheck(
                incognito_browser->tab_strip_model()->GetActiveWebContents()),
            kEarlyDataNotAcceptedTitle);
}

// Creating arbitrary user profiles via `ProfileManager` is not supported on
// ChromeOS, so this test cannot work there.
IN_PROC_BROWSER_TEST_P(TLS13EarlyDataPolicyEnabledByDefaultTest,
                       DisableWithNewRegularProfile) {
  PolicyMap policies;
  SetPolicy(&policies, key::kTLS13EarlyDataEnabled, base::Value(false));
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(GetLocalStateBooleanPref(prefs::kTLS13EarlyDataEnabled));
  content::FlushNetworkServiceInstanceForTesting();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->user_data_dir().AppendASCII("New Profile");

  base::test::TestFuture<Profile*> profile_future;
  profile_manager->CreateProfileAsync(profile_path,
                                      profile_future.GetCallback());
  Profile* new_profile = profile_future.Get();

  Browser* new_browser = CreateBrowser(new_profile);

  auto* render_frame_host = ui_test_utils::NavigateToURLWithDisposition(
      new_browser, GetTestPageURL(), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(render_frame_host);

  EXPECT_EQ(FetchResourceForEarlyDataCheck(
                new_browser->tab_strip_model()->GetActiveWebContents()),
            kEarlyDataNotAcceptedTitle);
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace policy
