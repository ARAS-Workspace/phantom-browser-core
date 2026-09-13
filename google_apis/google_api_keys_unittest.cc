// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for functions in google_apis/google_api_keys.h.
//
// Because the file deals with a lot of preprocessor defines and
// optionally includes an internal header, the way we test is by
// including the .cc file multiple times with different defines set.
// This is a little unorthodox, but it lets us test the behavior as
// close to unmodified as possible.


#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/version_info/channel.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "google_apis/api_key_cache.h"
#include "google_apis/default_api_keys.h"
#include "google_apis/gaia/gaia_config.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/google_api_keys.h"

#if defined(USE_OFFICIAL_GOOGLE_API_KEYS)
// Test official build behavior, since we are in a checkout where this
// is possible.
namespace official_build {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET
#undef GOOGLE_CDM_SERVER_CERTIFICATE

// Try setting some keys, these should be ignored since it's a build
// with official keys.
#define GOOGLE_API_KEY "bogus api_key"
#define GOOGLE_CLIENT_ID_MAIN "bogus client_id_main"

// Undef include guard so things get defined again, within this namespace.
#undef GOOGLE_APIS_INTERNAL_GOOGLE_CHROME_API_KEYS_
#undef GOOGLE_APIS_INTERNAL_METRICS_SIGNING_KEY_H_
#include "google_apis/internal/google_chrome_api_keys.h"
#include "google_apis/internal/metrics_signing_key.h"

// This file must be included after the internal files defining official keys.
#include "google_apis/default_api_keys-inc.cc"

}  // namespace official_build

TEST(GoogleAPIKeysTest, OfficialKeys) {
  google_apis::ApiKeyCache api_key_cache(
      official_build::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
  EXPECT_TRUE(google_apis::HasOAuthClientConfigured());
  EXPECT_TRUE(google_apis::IsGoogleChromeAPIKeyUsed());

  std::string api_key = google_apis::GetAPIKey();
  std::string id_main =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  std::string secret_main =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);
  std::string id_remoting =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING);
  std::string secret_remoting =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING);
  std::string id_remoting_host =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST);
#if BUILDFLAG(SUPPORT_CDM_SERVER_CERTIFICATE)
  std::string cdm_certificate = google_apis::GetCdmServerCertificate();
#endif

  EXPECT_NE(0u, api_key.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, api_key);
  EXPECT_NE("bogus api_key", api_key);

  EXPECT_NE(0u, id_main.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, id_main);
  EXPECT_NE("bogus client_id_main", id_main);

  EXPECT_NE(0u, secret_main.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, secret_main);

  EXPECT_NE(0u, id_remoting.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, id_remoting);

  EXPECT_NE(0u, secret_remoting.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting);

  EXPECT_NE(0u, id_remoting_host.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, id_remoting_host);

  EXPECT_NE(0u, secret_remoting_host.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting_host);

#if BUILDFLAG(SUPPORT_CDM_SERVER_CERTIFICATE)
  EXPECT_NE(0u, cdm_certificate.size());
  EXPECT_NE(google_apis::DefaultApiKeys::kUnsetApiToken, cdm_certificate);
#endif
}

#endif  // defined(USE_OFFICIAL_GOOGLE_API_KEYS)

// After this test, for the remainder of this compilation unit, we
// need official keys to not be used.
#undef BUILDFLAG_INTERNAL_CHROMIUM_BRANDING
#undef BUILDFLAG_INTERNAL_GOOGLE_CHROME_BRANDING
#define BUILDFLAG_INTERNAL_CHROMIUM_BRANDING() (1)
#define BUILDFLAG_INTERNAL_GOOGLE_CHROME_BRANDING() (0)
#undef USE_OFFICIAL_GOOGLE_API_KEYS

// Test the set of keys temporarily baked into Chromium by default.
namespace default_keys {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET
#undef GOOGLE_CDM_SERVER_CERTIFICATE

#include "google_apis/default_api_keys-inc.cc"

}  // namespace default_keys

TEST(GoogleAPIKeysTest, DefaultKeys) {
  google_apis::ApiKeyCache api_key_cache(
      default_keys::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_FALSE(google_apis::HasAPIKeyConfigured());
  EXPECT_FALSE(google_apis::HasOAuthClientConfigured());
  EXPECT_FALSE(google_apis::IsGoogleChromeAPIKeyUsed());

  std::string api_key = google_apis::GetAPIKey();
  std::string id_main =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  std::string secret_main =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);
  std::string id_remoting =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING);
  std::string secret_remoting =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING);
  std::string id_remoting_host =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST);

  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, api_key);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, id_main);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_main);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, id_remoting);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, id_remoting_host);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting_host);
#if BUILDFLAG(SUPPORT_CDM_SERVER_CERTIFICATE)
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken,
            google_apis::GetCdmServerCertificate());
#endif
}

// Override a couple of keys, leave the rest default.
namespace override_some_keys {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET
#undef GOOGLE_CDM_SERVER_CERTIFICATE

#define GOOGLE_API_KEY "API_KEY override"
#define GOOGLE_CLIENT_ID_REMOTING "CLIENT_ID_REMOTING override"

#include "google_apis/default_api_keys-inc.cc"

}  // namespace override_some_keys

TEST(GoogleAPIKeysTest, OverrideSomeKeys) {
  google_apis::ApiKeyCache api_key_cache(
      override_some_keys::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
  EXPECT_FALSE(google_apis::HasOAuthClientConfigured());
  EXPECT_FALSE(google_apis::IsGoogleChromeAPIKeyUsed());

  std::string api_key = google_apis::GetAPIKey();
  std::string id_main =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  std::string secret_main =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);
  std::string id_remoting =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING);
  std::string secret_remoting =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING);
  std::string id_remoting_host =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST);

  EXPECT_EQ("API_KEY override", api_key);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, id_main);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_main);
  EXPECT_EQ("CLIENT_ID_REMOTING override", id_remoting);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, id_remoting_host);
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken, secret_remoting_host);
#if BUILDFLAG(SUPPORT_CDM_SERVER_CERTIFICATE)
  EXPECT_EQ(google_apis::DefaultApiKeys::kUnsetApiToken,
            google_apis::GetCdmServerCertificate());
#endif
}

// Override all keys.
namespace override_all_keys {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET
#undef GOOGLE_CDM_SERVER_CERTIFICATE

#define GOOGLE_API_KEY "API_KEY"
#define GOOGLE_CLIENT_ID_MAIN "ID_MAIN"
#define GOOGLE_CLIENT_SECRET_MAIN "SECRET_MAIN"
#define GOOGLE_CLIENT_ID_REMOTING "ID_REMOTING"
#define GOOGLE_CLIENT_SECRET_REMOTING "SECRET_REMOTING"
#define GOOGLE_CLIENT_ID_REMOTING_HOST "ID_REMOTING_HOST"
#define GOOGLE_CLIENT_SECRET_REMOTING_HOST "SECRET_REMOTING_HOST"
#define GOOGLE_CDM_SERVER_CERTIFICATE "CDM_SERVER_CERTIFICATE"

#include "google_apis/default_api_keys-inc.cc"

}  // namespace override_all_keys

TEST(GoogleAPIKeysTest, OverrideAllKeys) {
  google_apis::ApiKeyCache api_key_cache(
      override_all_keys::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
  EXPECT_TRUE(google_apis::HasOAuthClientConfigured());
  EXPECT_FALSE(google_apis::IsGoogleChromeAPIKeyUsed());

  std::string api_key = google_apis::GetAPIKey();
  std::string id_main =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  std::string secret_main =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);
  std::string id_remoting =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING);
  std::string secret_remoting =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING);
  std::string id_remoting_host =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST);

  EXPECT_EQ("API_KEY", api_key);
  EXPECT_EQ("ID_MAIN", id_main);
  EXPECT_EQ("SECRET_MAIN", secret_main);
  EXPECT_EQ("ID_REMOTING", id_remoting);
  EXPECT_EQ("SECRET_REMOTING", secret_remoting);
  EXPECT_EQ("ID_REMOTING_HOST", id_remoting_host);
  EXPECT_EQ("SECRET_REMOTING_HOST", secret_remoting_host);
#if BUILDFLAG(SUPPORT_CDM_SERVER_CERTIFICATE)
  EXPECT_EQ("CDM_SERVER_CERTIFICATE", google_apis::GetCdmServerCertificate());
#endif
}

#if BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY)
// Override all keys using both preprocessor defines and setters.
// Setters should win.
namespace override_all_keys_setters {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET
#undef GOOGLE_CDM_SERVER_CERTIFICATE

#define GOOGLE_API_KEY "API_KEY"
#define GOOGLE_CLIENT_ID_MAIN "ID_MAIN"
#define GOOGLE_CLIENT_SECRET_MAIN "SECRET_MAIN"
#define GOOGLE_CLIENT_ID_REMOTING "ID_REMOTING"
#define GOOGLE_CLIENT_SECRET_REMOTING "SECRET_REMOTING"
#define GOOGLE_CLIENT_ID_REMOTING_HOST "ID_REMOTING_HOST"
#define GOOGLE_CLIENT_SECRET_REMOTING_HOST "SECRET_REMOTING_HOST"
#define GOOGLE_CDM_SERVER_CERTIFICATE "CDM_SERVER_CERTIFICATE"

#include "google_apis/default_api_keys-inc.cc"

}  // namespace override_all_keys_setters

TEST(GoogleAPIKeysTest, OverrideAllKeysUsingSetters) {
  google_apis::ApiKeyCache api_key_cache(
      override_all_keys_setters::GetDefaultApiKeysFromDefinedValues());
  auto scoped_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  std::string api_key("setter-API_KEY");
  std::string client_id("setter-CLIENT_ID");
  std::string client_secret("setter-CLIENT_SECRET");
  google_apis::InitializeAndOverrideAPIKeyAndOAuthClient(api_key, client_id,
                                                         client_secret);

  EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
  EXPECT_TRUE(google_apis::HasOAuthClientConfigured());
  EXPECT_FALSE(google_apis::IsGoogleChromeAPIKeyUsed());

  EXPECT_EQ(api_key, google_apis::GetAPIKey(::version_info::Channel::STABLE));
  EXPECT_EQ(api_key, google_apis::GetAPIKey());

  EXPECT_EQ(client_id,
            google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN));
  EXPECT_EQ(client_secret,
            google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN));

  EXPECT_EQ(client_id,
            google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING));
  EXPECT_EQ(client_secret,
            google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING));

  EXPECT_EQ(client_id,
            google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST));
  EXPECT_EQ(client_secret, google_apis::GetOAuth2ClientSecret(
                               google_apis::CLIENT_REMOTING_HOST));
#if BUILDFLAG(SUPPORT_CDM_SERVER_CERTIFICATE)
  std::string cdm_server_certificate("setter-CDM_SERVER_CERTIFICATE");
  EXPECT_EQ(cdm_server_certificate, google_apis::GetCdmServerCertificate());
#endif
}
#endif  // BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY)
