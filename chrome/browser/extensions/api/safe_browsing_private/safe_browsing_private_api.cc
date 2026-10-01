// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_api.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"

namespace extensions {


////////////////////////////////////////////////////////////////////////////////
// SafeBrowsingPrivateGetReferrerChainFunction

SafeBrowsingPrivateGetReferrerChainFunction::
    SafeBrowsingPrivateGetReferrerChainFunction() = default;

SafeBrowsingPrivateGetReferrerChainFunction::
    ~SafeBrowsingPrivateGetReferrerChainFunction() = default;

ExtensionFunction::ResponseAction
SafeBrowsingPrivateGetReferrerChainFunction::Run() {
  std::optional<api::safe_browsing_private::GetReferrerChain::Params> params =
      api::safe_browsing_private::GetReferrerChain::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* contents = nullptr;

  if (!ExtensionTabUtil::GetTabById(params->tab_id, browser_context(),
                                    include_incognito_information(),
                                    &contents)) {
    return RespondNow(Error(
        base::StringPrintf("Could not find tab with id %d.", params->tab_id)));
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions
