// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/webid_test_content_browser_client.h"

#include "content/browser/webid/test/mock_modal_dialog_view_delegate.h"

namespace content::webid {

WebIdTestContentBrowserClient::WebIdTestContentBrowserClient() = default;
WebIdTestContentBrowserClient::~WebIdTestContentBrowserClient() = default;

std::unique_ptr<IdentityRequestDialogController>
WebIdTestContentBrowserClient::CreateIdentityRequestDialogController(
    WebContents* rp_web_contents) {
  DCHECK(test_dialog_controller_);
  return std::move(test_dialog_controller_);
}

void WebIdTestContentBrowserClient::SetIdentityRequestDialogController(
    std::unique_ptr<IdentityRequestDialogController> controller) {
  test_dialog_controller_ = std::move(controller);
}

void WebIdTestContentBrowserClient::SetIdentityRegistry(
    WebContents* web_contents,
    base::WeakPtr<IdentityRegistryDelegate> delegate,
    const GURL& config_url) {
  IdentityRegistry::CreateForWebContents(web_contents, delegate, config_url);
}

}  // namespace content::webid
