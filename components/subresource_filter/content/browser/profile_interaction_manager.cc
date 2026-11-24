// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/profile_interaction_manager.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/content/browser/utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace subresource_filter {

ProfileInteractionManager::ProfileInteractionManager(
    SubresourceFilterProfileContext* profile_context)
    : profile_context_(profile_context) {}

ProfileInteractionManager::~ProfileInteractionManager() = default;

void ProfileInteractionManager::DidCreatePage(content::Page& page) {
  // A new ProfileInteractionManager is created for each page so we should only
  // call this, at most, once.
  CHECK(!page_);
  page_ = &page;
}

void ProfileInteractionManager::OnReloadRequested() {
  // A reload request comes from browser so it will always be associated with
  // the primary page.
  CHECK(page_);
  CHECK(page_->IsPrimary());

  profile_context_->settings_manager()->AllowlistSite(
      page_->GetMainDocument().GetLastCommittedURL());

  // Since the reload comes from the primary page, the use of WebContents here
  // is correct.
  GetWebContents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

void ProfileInteractionManager::MaybeShowNotification() {
  // The caller should make sure this is only called from pages that are
  // currently primary.
  CHECK(page_);
  CHECK(page_->IsPrimary());

  const GURL& top_level_url = page_->GetMainDocument().GetLastCommittedURL();
  if (profile_context_->settings_manager()->ShouldShowUIForSite(
          top_level_url)) {
    // TODO(crbug.com/40139135): Plumb the actual frame reference here
    // (it comes from
    // ContentSubresourceFilterThrottleManager::DidDisallowFirstSubresource,
    // which comes from a specific frame).
    content_settings::PageSpecificContentSettings* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            &page_->GetMainDocument());
    content_settings->OnContentBlocked(ContentSettingsType::ADS);

    ContentSubresourceFilterThrottleManager::LogAction(
        SubresourceFilterAction::kUIShown);
    profile_context_->settings_manager()->OnDidShowUI(top_level_url);
  } else {
    ContentSubresourceFilterThrottleManager::LogAction(
        SubresourceFilterAction::kUISuppressed);
  }
}

content_settings::CookieSettings*
ProfileInteractionManager::GetCookieSettings() {
  return profile_context_->cookie_settings();
}

content::WebContents* ProfileInteractionManager::GetWebContents() {
  CHECK(page_);
  CHECK(page_->IsPrimary());
  return content::WebContents::FromRenderFrameHost(&page_->GetMainDocument());
}

}  // namespace subresource_filter
