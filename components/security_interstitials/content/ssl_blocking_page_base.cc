// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/ssl_blocking_page_base.h"

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/controller_client.h"

SSLBlockingPageBase::SSLBlockingPageBase(
    content::WebContents* web_contents,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool overridable,
    const base::Time& time_triggered,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)) {}

SSLBlockingPageBase::~SSLBlockingPageBase() = default;

void SSLBlockingPageBase::OnInterstitialClosing() {}

void SSLBlockingPageBase::DisableExtendedReportingCheckbox(
    base::DictValue& load_time_data) {
  load_time_data.Set(security_interstitials::kDisplayCheckBox, false);
}
