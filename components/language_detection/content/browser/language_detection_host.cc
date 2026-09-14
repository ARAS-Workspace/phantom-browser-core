// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/content/browser/language_detection_host.h"

#include <string>
#include <utility>

#include "components/language_detection/core/language_detection_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"

namespace language_detection {

LanguageDetectionHost::LanguageDetectionHost(content::WebContents* web_contents)
    : content::WebContentsUserData<LanguageDetectionHost>(*web_contents),
      content::WebContentsObserver(web_contents) {}

LanguageDetectionHost::~LanguageDetectionHost() = default;

void LanguageDetectionHost::AddReceiver(
    mojo::PendingReceiver<mojom::LanguageDetectionHost> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void LanguageDetectionHost::LanguageDetermined(
    const LanguageDetectionDetails& details) {
  last_details_ = details;
  NotifyLanguageDetermined(details);
}

void LanguageDetectionHost::NotifyLanguageDetermined(
    const LanguageDetectionDetails& details) {
  for (auto& observer : language_detection_observers()) {
    observer.OnLanguageDetermined(details);
  }
}

std::string LanguageDetectionHost::adopted_language() const {
  return last_details_ ? last_details_->adopted_language : std::string();
}

void LanguageDetectionHost::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  // A reload brings back the same document, so the language detected on it
  // still describes what the tab shows.
  if (navigation_handle->GetReloadType() != content::ReloadType::NONE) {
    return;
  }
  last_details_.reset();
}

std::string GetAdoptedLanguage(content::WebContents* web_contents) {
  LanguageDetectionHost* host =
      web_contents ? LanguageDetectionHost::FromWebContents(web_contents)
                   : nullptr;
  return host ? host->adopted_language() : std::string();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LanguageDetectionHost);

}  // namespace language_detection
