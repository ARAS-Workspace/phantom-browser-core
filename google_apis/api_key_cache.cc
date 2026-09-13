// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/api_key_cache.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/logging.h"
#include "base/strings/cstring_view.h"
#include "base/strings/stringize_macros.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "google_apis/buildflags.h"
#include "google_apis/default_api_keys.h"
#include "google_apis/gaia/gaia_switches.h"

namespace google_apis {

namespace {

// Gets a value for a key: the value baked into the build, or the default when
// that value is unset. The key cannot be overridden at run time.
static std::string CalculateKeyValue(const char* baked_in_value,
                                     base::cstring_view key_name,
                                     const std::string& default_if_unset,
                                     bool allow_unset_values) {
  std::string key_value = baked_in_value;

  if (key_value == DefaultApiKeys::kUnsetApiToken) {
    // No key should be unset in an official build except the
    // GOOGLE_DEFAULT_* keys. The default keys don't trigger this
    // check as their "unset" value is not DefaultApiKeys::kUnsetApiToken.
    CHECK(allow_unset_values);
    if (default_if_unset.size() > 0) {
      VLOG(1) << "Using default value \"" << default_if_unset
              << "\" for API key " << key_name;
      key_value = default_if_unset;
    }
  }

  // This should remain a debug-only log.
  DVLOG(1) << "API key " << key_name << "=" << key_value;

  return key_value;
}
}  // namespace


ApiKeyCache::ApiKeyCache(const DefaultApiKeys& default_api_keys)
    : is_initialized_using_google_chrome_keys_(
          default_api_keys.is_using_google_chrome_keys) {
  api_key_ = CalculateKeyValue(
      default_api_keys.google_api_key, STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY),
      std::string(), default_api_keys.allow_unset_values);
// A special non-stable key is at the moment defined only for Android Chrome.
#if BUILDFLAG(IS_ANDROID)
  api_key_non_stable_ = CalculateKeyValue(
      default_api_keys.google_api_key_android_non_stable,
      STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_ANDROID_NON_STABLE), std::string(),
      default_api_keys.allow_unset_values);
#else
  api_key_non_stable_ = api_key_;
#endif

  api_key_remoting_ =
      CalculateKeyValue(default_api_keys.google_api_key_remoting,
                        STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_REMOTING),
                        std::string(), default_api_keys.allow_unset_values);

  api_key_soda_ =
      CalculateKeyValue(default_api_keys.google_api_key_soda,
                        STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_SODA),
                        std::string(), default_api_keys.allow_unset_values);

  api_key_partial_translate_ = CalculateKeyValue(
      default_api_keys.google_api_key_partial_translate,
      STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_PARTIAL_TRANSLATE), std::string(),
      default_api_keys.allow_unset_values);
#if !BUILDFLAG(IS_ANDROID)
  api_key_hats_ =
      CalculateKeyValue(default_api_keys.google_api_key_hats,
                        STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY_HATS),
                        std::string(), default_api_keys.allow_unset_values);
#endif

  metrics_key_ =
      CalculateKeyValue(default_api_keys.google_metrics_signing_key,
                        STRINGIZE_NO_EXPANSION(GOOGLE_METRICS_SIGNING_KEY),
                        std::string(), default_api_keys.allow_unset_values);

#if BUILDFLAG(SUPPORT_CDM_SERVER_CERTIFICATE)
  // As the CDM server certificate is only used for a prototype feature,
  // we allow unset values.
  cdm_server_certificate_ =
      CalculateKeyValue(default_api_keys.google_cdm_server_certificate,
                        STRINGIZE_NO_EXPANSION(GOOGLE_CDM_SERVER_CERTIFICATE),
                        std::string(), /*allow_unset_values=*/true);
#endif

  std::string default_client_id =
      CalculateKeyValue(default_api_keys.google_default_client_id,
                        STRINGIZE_NO_EXPANSION(GOOGLE_DEFAULT_CLIENT_ID),
                        std::string(), default_api_keys.allow_unset_values);
  std::string default_client_secret =
      CalculateKeyValue(default_api_keys.google_default_client_secret,
                        STRINGIZE_NO_EXPANSION(GOOGLE_DEFAULT_CLIENT_SECRET),
                        std::string(), default_api_keys.allow_unset_values);

  // We currently only allow overriding the baked-in values for the
  // default OAuth2 client ID and secret using a command-line
  // argument and gaia config, since that is useful to enable testing against
  // staging servers, and since that was what was possible and
  // likely practiced by the QA team before this implementation was
  // written.
  client_ids_[CLIENT_MAIN] =
      CalculateKeyValue(default_api_keys.google_client_id_main,
                        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_MAIN),
                        default_client_id, default_api_keys.allow_unset_values);
  client_secrets_[CLIENT_MAIN] = CalculateKeyValue(
      default_api_keys.google_client_secret_main,
      STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_MAIN), default_client_secret,
      default_api_keys.allow_unset_values);

  client_ids_[CLIENT_REMOTING] =
      CalculateKeyValue(default_api_keys.google_client_id_remoting,
                        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_REMOTING),
                        default_client_id, default_api_keys.allow_unset_values);
  client_secrets_[CLIENT_REMOTING] = CalculateKeyValue(
      default_api_keys.google_client_secret_remoting,
      STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_REMOTING),
      default_client_secret, default_api_keys.allow_unset_values);

  client_ids_[CLIENT_REMOTING_HOST] =
      CalculateKeyValue(default_api_keys.google_client_id_remoting_host,
                        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_REMOTING_HOST),
                        default_client_id, default_api_keys.allow_unset_values);
  client_secrets_[CLIENT_REMOTING_HOST] = CalculateKeyValue(
      default_api_keys.google_client_secret_remoting_host,
      STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_REMOTING_HOST),
      default_client_secret, default_api_keys.allow_unset_values);
}

ApiKeyCache::~ApiKeyCache() = default;

const std::string& ApiKeyCache::GetClientID(OAuth2Client client) const {
  DCHECK_LT(client, CLIENT_NUM_ITEMS);
  return client_ids_[client];
}

const std::string& ApiKeyCache::GetClientSecret(OAuth2Client client) const {
  DCHECK_LT(client, CLIENT_NUM_ITEMS);
  return client_secrets_[client];
}

#if BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY)
void ApiKeyCache::SetClientID(OAuth2Client client,
                              const std::string& client_id) {
  client_ids_[client] = client_id;
}

void ApiKeyCache::SetClientSecret(OAuth2Client client,
                                  const std::string& client_secret) {
  client_secrets_[client] = client_secret;
}
#endif  // BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY)

bool ApiKeyCache::HasAPIKeyConfigured() const {
  return api_key_ != DefaultApiKeys::kUnsetApiToken;
}

bool ApiKeyCache::HasOAuthClientConfigured() const {
  auto is_unset = [](const std::string& value) {
    return value == DefaultApiKeys::kUnsetApiToken;
  };
  return std::ranges::none_of(client_ids_, is_unset) &&
         std::ranges::none_of(client_secrets_, is_unset);
}

bool ApiKeyCache::IsGoogleChromeAPIKeyUsed() const {
  return is_initialized_using_google_chrome_keys_;
}

}  // namespace google_apis
