// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_client.h"

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"

namespace autofill {

ContentAutofillClient::ContentAutofillClient(content::WebContents* web_contents)
    : content::WebContentsUserData<ContentAutofillClient>(*web_contents),
      autofill_driver_factory_(web_contents, this) {}

ContentAutofillClient::~ContentAutofillClient() = default;

content::WebContents* ContentAutofillClient::web_contents() const {
  return autofill_driver_factory_.web_contents();
}

ContentAutofillDriverFactory&
ContentAutofillClient::GetAutofillDriverFactory() {
  return autofill_driver_factory_;
}

std::u16string_view ContentAutofillClient::GetPageTitle() const {
  CHECK(web_contents());
  return web_contents()->GetTitle();
}

bool ContentAutofillClient::DocumentUsedWebOTP() {
  return GetWebContents().GetPrimaryMainFrame()->DocumentUsedWebOTP();
}

AutofillManager*
ContentAutofillClient::GetAutofillManagerForPrimaryMainFrame() {
  if (auto* driver = ContentAutofillDriver::GetForRenderFrameHost(
          GetWebContents().GetPrimaryMainFrame())) {
    return &driver->GetAutofillManager();
  }
  return nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentAutofillClient);

}  // namespace autofill
