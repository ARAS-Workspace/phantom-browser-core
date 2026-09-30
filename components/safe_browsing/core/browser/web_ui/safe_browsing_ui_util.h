// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_UI_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_UI_UTIL_H_

#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/download_check_result.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace internal {
struct ReferringAppInfo;
}  // namespace internal
class SafeBrowsingUIHandler;
class WebUIInfoSingletonEventObserver;
}  // namespace safe_browsing

namespace safe_browsing::web_ui {

// The struct to combine a PhishGuard request and the token associated
// with it. The token is not part of the request proto because it is sent in the
// header. The token will be displayed along with the request in the safe
// browsing page.
struct LoginReputationClientRequestAndToken {
  LoginReputationClientRequest request;
  std::string token;
};

// The struct to combine a URL real time lookup request and the token associated
// with it. The token is not part of the request proto because it is sent in the
// header. The token will be displayed along with the request in the safe
// browsing page.
struct URTLookupRequest {
  RTLookupRequest request;
  std::string token;
};

// Combines the inner request (SearchHashesRequest) sent to Safe Browsing with
// other details about the outer request (the relay URL + the OHTTP key used
// for encryption). All are displayed on chrome://safe-browsing.
struct HPRTLookupRequest {
  V5::SearchHashesRequest inner_request;
  std::string relay_url_spec;
  std::string ohttp_key;
};

// The struct to combine a client-side phishing request and the token associated
// with it. The token is not part of the request proto because it is sent in the
// header. The token will be displayed along with the request in the safe
// browsing page.
struct ClientPhishingRequestAndToken {
  ClientPhishingRequest request;
  std::string token;
};

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)

std::string UserReadableTimeFromMillisSinceEpoch(int64_t time_in_milliseconds);
void AddStoreInfo(
    const DatabaseManagerInfo::DatabaseInfo::StoreInfo& store_info,
    base::ListValue& database_info_list);

void AddDatabaseInfo(const DatabaseManagerInfo::DatabaseInfo& database_info,
                     base::ListValue& database_info_list);

void AddUpdateInfo(const DatabaseManagerInfo::UpdateInfo& update_info,
                   base::ListValue& database_info_list);

void ParseFullHashInfo(
    const FullHashCacheInfo::FullHashCache::CachedHashPrefixInfo::FullHashInfo&
        full_hash_info,
    base::DictValue& full_hash_info_dict);

void ParseFullHashCache(const FullHashCacheInfo::FullHashCache& full_hash_cache,
                        base::ListValue& full_hash_cache_list);
void ParseFullHashCacheInfo(const FullHashCacheInfo& full_hash_cache_info_proto,
                            base::ListValue& full_hash_cache_info);

std::string AddFullHashCacheInfo(
    const FullHashCacheInfo& full_hash_cache_info_proto);

#endif

// Serialization helper functions.
std::string SerializeClientDownloadRequest(const ClientDownloadRequest& cdr);
std::string SerializeClientDownloadResponse(const ClientDownloadResponse& cdr);
std::string SerializeClientPhishingRequest(
    const ClientPhishingRequestAndToken& cprat);
std::string SerializeClientPhishingResponse(const ClientPhishingResponse& cpr);
std::string SerializeCSBRR(const ClientSafeBrowsingReportRequest& report);
std::string SerializeDownloadUrlChecked(const std::vector<GURL>& urls,
                                        DownloadCheckResult result);
std::string SerializeJson(base::ValueView value);
base::DictValue SerializePGEvent(const sync_pb::UserEventSpecifics& event);
base::DictValue SerializeSecurityEvent(const sync_pb::GaiaPasswordReuse& event);
std::string SerializePGPing(
    const LoginReputationClientRequestAndToken& request_and_token);
std::string SerializePGResponse(const LoginReputationClientResponse& response);
std::string SerializeURTLookupPing(const URTLookupRequest& ping);
std::string SerializeURTLookupResponse(const RTLookupResponse& response);
std::string SerializeHPRTLookupPing(const HPRTLookupRequest& ping);
std::string SerializeHPRTLookupResponse(
    const V5::SearchHashesResponse& response);
base::DictValue SerializeLogMessage(base::Time timestamp,
                                    const std::string& message);
base::DictValue SerializeReportingEvent(const base::DictValue& event);
base::DictValue SerializeUploadEventsRequest(
    const ::chrome::cros::reporting::proto::UploadEventsRequest&
        upload_events_request,
    const base::DictValue& result);

}  // namespace safe_browsing::web_ui

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_UI_UTIL_H_
