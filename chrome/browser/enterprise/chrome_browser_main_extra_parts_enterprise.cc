// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/chrome_browser_main_extra_parts_enterprise.h"

#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "content/public/browser/browser_thread.h"

namespace enterprise_util {

namespace {

}  // namespace

ChromeBrowserMainExtraPartsEnterprise::ChromeBrowserMainExtraPartsEnterprise() =
    default;

ChromeBrowserMainExtraPartsEnterprise::
    ~ChromeBrowserMainExtraPartsEnterprise() = default;

void ChromeBrowserMainExtraPartsEnterprise::PostCreateMainMessageLoop() {
  // Set up and register ERP reporting client.
  reporting_client_ =
      reporting::ReportingClient::Create(content::GetUIThreadTaskRunner({}));
}

}  // namespace enterprise_util
