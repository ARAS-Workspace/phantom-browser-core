// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_network_context_client.h"

#include "build/build_config.h"
#include "net/base/net_errors.h"

namespace content {

PrefetchNetworkContextClient::PrefetchNetworkContextClient() = default;
PrefetchNetworkContextClient::~PrefetchNetworkContextClient() = default;

void PrefetchNetworkContextClient::OnFileUploadRequested(
    const network::OriginatingProcessId& process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    const GURL& destination_url,
    OnFileUploadRequestedCallback callback) {
  std::move(callback).Run(net::ERR_ACCESS_DENIED, std::vector<base::File>());
}

void PrefetchNetworkContextClient::OnCanSendReportingReports(
    const std::vector<url::Origin>& origins,
    OnCanSendReportingReportsCallback callback) {
  std::move(callback).Run(std::vector<url::Origin>());
}

void PrefetchNetworkContextClient::OnCanSendDomainReliabilityUpload(
    const url::Origin& origin,
    OnCanSendDomainReliabilityUploadCallback callback) {
  std::move(callback).Run(false);
}

#if BUILDFLAG(IS_CT_SUPPORTED)
void PrefetchNetworkContextClient::OnCanSendSCTAuditingReport(
    OnCanSendSCTAuditingReportCallback callback) {
  std::move(callback).Run(false);
}

void PrefetchNetworkContextClient::OnNewSCTAuditingReportSent() {}
#endif

}  // namespace content
