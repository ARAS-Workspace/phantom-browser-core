// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_
#define GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_

#include <memory>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"

namespace network {
class SharedURLLoaderFactory;
}

// A helper class to fetch user information and account capabilities.
//
// Supports one request at a time; for parallel requests, create multiple
// instances.
namespace gaia {

class COMPONENT_EXPORT(GOOGLE_APIS) GaiaOAuthClient {
 public:
  class COMPONENT_EXPORT(GOOGLE_APIS) Delegate {
   public:
    // Invoked on a successful response to the GetUserInfo request.
    virtual void OnGetUserInfoResponse(const base::DictValue& user_info) {}
    virtual void OnGetAccountCapabilitiesResponse(
        const base::DictValue& account_capabilities) {}
    // Invoked when there is an OAuth error with one of the requests.
    virtual void OnOAuthError() = 0;
    // Invoked when there is a network error or upon receiving an invalid
    // response. This is invoked when the maximum number of retries have been
    // exhausted. If max_retries is -1, this is never invoked.
    virtual void OnNetworkError(int response_code) = 0;

   protected:
    virtual ~Delegate() {}
  };

  GaiaOAuthClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  GaiaOAuthClient(const GaiaOAuthClient&) = delete;
  GaiaOAuthClient& operator=(const GaiaOAuthClient&) = delete;

  ~GaiaOAuthClient();

  // In the below methods, |max_retries| specifies the maximum number of times
  // we should retry on a network error in invalid response. This does not
  // apply in the case of an OAuth error (i.e. there was something wrong with
  // the input arguments). Setting |max_retries| to -1 implies infinite retries.

  // Call the userinfo API, returning all the user info associated
  // with the given access token. The provided access token must have
  // https://www.googleapis.com/auth/userinfo.profile in its scopes.  If
  // email addresses are also to be retrieved, then
  // https://www.googleapis.com/auth/userinfo.email must also be specified.
  // See |max_retries| docs above.
  void GetUserInfo(const std::string& oauth_access_token,
                   int max_retries,
                   Delegate* delegate);

  // Call the account capabilities API, returning a dictionary of response
  // values. The provided access token must have
  // https://www.googleapis.com/auth/account.capabilities in its scopes. See
  // |max_retries| docs above.
  // When `gaia::features::kGetAccountCapabilitiesUsesGetAllVisibleUrl` is
  // enabled, this fetches all available capabilities and `capabilities_names`
  // is ignored. Otherwise, this only fetches values for capabilities listed in
  // `capabilities_names`.
  void GetAccountCapabilities(
      const std::string& oauth_access_token,
      base::span<const std::string_view> capabilities_names,
      int max_retries,
      Delegate* delegate);

 private:
  // The guts of the implementation live in this class.
  class Core;
  scoped_refptr<Core> core_;
};

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_
