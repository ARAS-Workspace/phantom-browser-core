// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_oauth_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_features.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/backoff_entry.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace gaia {

class GaiaOAuthClient::Core
    : public base::RefCountedThreadSafe<GaiaOAuthClient::Core> {
 public:
  Core(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : backoff_entry_(&backoff_policy_),
        num_retries_(0),
        max_retries_(0),
        url_loader_factory_(url_loader_factory),
        delegate_(nullptr),
        request_type_(NO_PENDING_REQUEST) {
    backoff_policy_.num_errors_to_ignore = 2;
    backoff_policy_.initial_delay_ms = 700;
    backoff_policy_.multiply_factor = 1.4;
    backoff_policy_.jitter_factor = 0.4;
    backoff_policy_.maximum_backoff_ms = 15 * 60 * 1000;
    backoff_policy_.entry_lifetime_ms = 2 * 60 * 1000;
    backoff_policy_.always_use_initial_delay = false;
  }

  void GetUserInfo(const std::string& oauth_access_token,
                   int max_retries,
                   Delegate* delegate);
  void GetAccountCapabilities(
      const std::string& oauth_access_token,
      base::span<const std::string_view> capabilities_names,
      int max_retries,
      Delegate* delegate);
  // Called as a SimpleURLLoader callback
  void OnURLLoadComplete(std::optional<std::string> body);

 private:
  friend class base::RefCountedThreadSafe<Core>;

  enum RequestType {
    NO_PENDING_REQUEST,
    USER_INFO,
    ACCOUNT_CAPABILITIES,
  };

  ~Core() {}

  // Stores request settings into |this| and calls SendRequest().
  void MakeRequest(
      RequestType type,
      const GURL& url,
      std::string post_body /* may be empty if not needed*/,
      std::string authorization_header /* empty if not needed */,
      std::string http_method_override_header /* empty if not needed */,
      int max_retries,
      GaiaOAuthClient::Delegate* delegate,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  // Sends out a request based on things MakeRequest() stored...
  // once |backoff_entry_| says it's OK. Can be called many times to retry,
  // assuming |request_| is destroyed first.
  void SendRequest();

  // Actually sends the request.
  void SendRequestImpl();

  void HandleResponse(std::optional<std::string> body,
                      bool* should_retry_request);

  net::BackoffEntry::Policy backoff_policy_;
  net::BackoffEntry backoff_entry_;

  int num_retries_;
  int max_retries_;
  GURL url_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  std::string post_body_;
  std::string authorization_header_;
  std::string http_method_override_header_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<GaiaOAuthClient::Delegate> delegate_;
  std::unique_ptr<network::SimpleURLLoader> request_;
  RequestType request_type_;

  base::WeakPtrFactory<Core> weak_ptr_factory_{this};
};

void GaiaOAuthClient::Core::GetUserInfo(const std::string& oauth_access_token,
                                        int max_retries,
                                        Delegate* delegate) {
  net::MutableNetworkTrafficAnnotationTag traffic_annotation(
      net::DefineNetworkTrafficAnnotation("gaia_oauth_client_get_user_info", R"(
        semantics {
          sender: "OAuth 2.0 calls"
          description:
            "This request is used to fetch profile information about the user, "
            "like the email, the ID of the account, the full name, and the "
            "profile picture."
          trigger:
            "The main trigger for this request is in the AccountTrackerService "
            "that fetches the user info soon after the user signs in."
          data:
            "The OAuth 2.0 access token of the account."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            BrowserSignin {
              BrowserSignin: 0
            }
          }
        })"));
  std::string auth = "OAuth " + oauth_access_token;
  MakeRequest(USER_INFO, GaiaUrls::GetInstance()->oauth_user_info_url(),
              /* post_body = */ std::string(), auth,
              /* http_method_override_header = */ std::string(), max_retries,
              delegate, traffic_annotation);
}

void GaiaOAuthClient::Core::GetAccountCapabilities(
    const std::string& oauth_access_token,
    base::span<const std::string_view> capabilities_names,
    int max_retries,
    Delegate* delegate) {
  std::string auth = base::StrCat({"Bearer ", oauth_access_token});

  net::MutableNetworkTrafficAnnotationTag traffic_annotation(
      net::DefineNetworkTrafficAnnotation(
          "gaia_oauth_client_get_account_capabilities",
          R"(
        semantics {
          sender: "OAuth 2.0 calls"
          description:
            "This request is used to fetch account capabilities. Capabilities "
            "provide information about state and features of Gaia accounts."
          trigger:
            "AccountTrackerService fetches account capabilities soon after the "
            "user signs in. Afterwards, AccountTrackerService periodically "
            "triggers this request to keep account capabilities up to date for "
            "existing accounts."
          data:
            "The OAuth 2.0 access token of the account and a predefined list "
            "of capabilities to fetch."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            BrowserSignin {
              BrowserSignin: 0
            }
          }
        })"));

  if (base::FeatureList::IsEnabled(
          gaia::features::kGetAccountCapabilitiesUsesGetAllVisibleUrl)) {
    MakeRequest(ACCOUNT_CAPABILITIES,
                GURL(GaiaUrls::GetInstance()
                         ->account_capabilities_get_all_visible_url()),
                /*post_body=*/std::string(), auth,
                /*http_method_override_header=*/std::string(), max_retries,
                delegate, traffic_annotation);
  } else {
    std::string post_body =
        base::StrCat({"names=", base::EscapeUrlEncodedData(
                                    *capabilities_names.begin(), true)});
    for (auto it = capabilities_names.begin() + 1;
         it != capabilities_names.end(); ++it) {
      base::StrAppend(&post_body,
                      {"&names=", base::EscapeUrlEncodedData(*it, true)});
    }

    MakeRequest(
        ACCOUNT_CAPABILITIES,
        GURL(GaiaUrls::GetInstance()->account_capabilities_batch_get_url()),
        post_body, auth, /*http_method_override_header=*/"GET", max_retries,
        delegate, traffic_annotation);
  }
}

void GaiaOAuthClient::Core::MakeRequest(
    RequestType type,
    const GURL& url,
    std::string post_body,
    std::string authorization_header,
    std::string http_method_override_header,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_EQ(request_type_, NO_PENDING_REQUEST);
  request_type_ = type;
  delegate_ = delegate;
  num_retries_ = 0;
  max_retries_ = max_retries;
  url_ = url;
  traffic_annotation_ = traffic_annotation;
  post_body_ = std::move(post_body);
  authorization_header_ = std::move(authorization_header);
  http_method_override_header_ = std::move(http_method_override_header);
  SendRequest();
}

void GaiaOAuthClient::Core::SendRequest() {
  if (backoff_entry_.ShouldRejectRequest()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GaiaOAuthClient::Core::SendRequestImpl,
                       weak_ptr_factory_.GetWeakPtr()),
        backoff_entry_.GetTimeUntilRelease());
  } else {
    SendRequestImpl();
  }
}

void GaiaOAuthClient::Core::SendRequestImpl() {
  DCHECK(!request_.get()) << "Tried to fetch two things at once!";

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->method = post_body_.empty() ? "GET" : "POST";
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
  if (!authorization_header_.empty()) {
    resource_request->headers.SetHeader("Authorization", authorization_header_);
  }
  if (!http_method_override_header_.empty()) {
    resource_request->headers.SetHeader("X-HTTP-Method-Override",
                                        http_method_override_header_);
  }

  request_ = network::SimpleURLLoader::Create(
      std::move(resource_request),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation_));

  if (!post_body_.empty()) {
    request_->AttachStringForUpload(post_body_,
                                    "application/x-www-form-urlencoded");
  }

  // Retry is implemented internally.
  request_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);

  request_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      // Unretained(this) is safe since |this| owns |request_|, and its deletion
      // will cancel the callback.
      base::BindOnce(&GaiaOAuthClient::Core::OnURLLoadComplete,
                     base::Unretained(this)));
}

void GaiaOAuthClient::Core::OnURLLoadComplete(std::optional<std::string> body) {
  bool should_retry = false;
  base::WeakPtr<GaiaOAuthClient::Core> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  // HandleResponse() may delete |this| if it assigns |should_retry| == false.
  HandleResponse(std::move(body), &should_retry);
  if (should_retry) {
    num_retries_++;
    backoff_entry_.InformOfRequest(false);
    SendRequest();
  } else {
    if (weak_this) {
      backoff_entry_.InformOfRequest(true);
    }
  }
}

void GaiaOAuthClient::Core::HandleResponse(std::optional<std::string> body,
                                           bool* should_retry_request) {
  *should_retry_request = false;
  // Move ownership of the request fetcher into a local scoped_ptr which
  // will be nuked when we're done handling the request.
  std::unique_ptr<network::SimpleURLLoader> source = std::move(request_);

  int response_code = -1;
  if (source->ResponseInfo() && source->ResponseInfo()->headers) {
    response_code = source->ResponseInfo()->headers->response_code();
  }

  // HTTP_BAD_REQUEST means the arguments are invalid.  HTTP_UNAUTHORIZED means
  // the access or refresh token is invalid. No point retrying. We are
  // done here.
  if (response_code == net::HTTP_BAD_REQUEST ||
      response_code == net::HTTP_UNAUTHORIZED) {
    delegate_->OnOAuthError();
    return;
  }

  std::optional<base::DictValue> response_dict;
  if (response_code == net::HTTP_OK && body) {
    response_dict =
        base::JSONReader::ReadDict(*body, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  }

  if (!response_dict) {
    // If we don't have an access token yet and the the error was not
    // RC_BAD_REQUEST, we may need to retry.
    if ((max_retries_ != -1) && (num_retries_ >= max_retries_)) {
      // Retry limit reached. Give up.
      request_type_ = NO_PENDING_REQUEST;
      delegate_->OnNetworkError(response_code);
    } else {
      *should_retry_request = true;
    }
    return;
  }

  RequestType type = request_type_;
  request_type_ = NO_PENDING_REQUEST;

  switch (type) {
    case USER_INFO: {
      delegate_->OnGetUserInfoResponse(*response_dict);
      break;
    }

    case ACCOUNT_CAPABILITIES: {
      delegate_->OnGetAccountCapabilitiesResponse(*response_dict);
      break;
    }

    case NO_PENDING_REQUEST:
      NOTREACHED();
  }
}

GaiaOAuthClient::GaiaOAuthClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  core_ = new Core(std::move(url_loader_factory));
}

GaiaOAuthClient::~GaiaOAuthClient() {}

void GaiaOAuthClient::GetUserInfo(const std::string& access_token,
                                  int max_retries,
                                  Delegate* delegate) {
  return core_->GetUserInfo(access_token, max_retries, delegate);
}

void GaiaOAuthClient::GetAccountCapabilities(
    const std::string& oauth_access_token,
    base::span<const std::string_view> capabilities_names,
    int max_retries,
    Delegate* delegate) {
  return core_->GetAccountCapabilities(oauth_access_token, capabilities_names,
                                       max_retries, delegate);
}

}  // namespace gaia
