// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/content/browser/language_detection_host.h"

#include "components/language_detection/core/language_detection_details.h"
#include "content/public/browser/web_contents.h"

namespace language_detection {

LanguageDetectionHost::LanguageDetectionHost(content::WebContents* web_contents)
    : content::WebContentsUserData<LanguageDetectionHost>(*web_contents) {}

LanguageDetectionHost::~LanguageDetectionHost() = default;

void LanguageDetectionHost::NotifyLanguageDetermined(
    const LanguageDetectionDetails& details) {
  for (auto& observer : language_detection_observers()) {
    observer.OnLanguageDetermined(details);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LanguageDetectionHost);

}  // namespace language_detection
