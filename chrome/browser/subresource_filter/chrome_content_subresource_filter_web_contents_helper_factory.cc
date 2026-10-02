// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/chrome_content_subresource_filter_web_contents_helper_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"

void CreateSubresourceFilterWebContentsHelper(
    content::WebContents* web_contents) {
  subresource_filter::RulesetService* ruleset_service =
      g_browser_process->subresource_filter_ruleset_service();
  subresource_filter::VerifiedRulesetDealer::Handle* dealer =
      ruleset_service ? ruleset_service->GetRulesetDealer() : nullptr;
  subresource_filter::ContentSubresourceFilterWebContentsHelper::
      CreateForWebContents(
          web_contents,
          SubresourceFilterProfileContextFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext())),
          dealer);
}
