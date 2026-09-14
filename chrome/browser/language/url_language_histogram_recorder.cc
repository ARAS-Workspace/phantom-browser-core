// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/url_language_histogram_recorder.h"

#include "chrome/browser/language/url_language_histogram_factory.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/language_detection/content/browser/language_detection_host.h"
#include "components/language_detection/core/language_detection_details.h"
#include "content/public/browser/web_contents.h"

UrlLanguageHistogramRecorder::UrlLanguageHistogramRecorder(
    content::WebContents* web_contents)
    : content::WebContentsUserData<UrlLanguageHistogramRecorder>(*web_contents),
      language_histogram_(UrlLanguageHistogramFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())) {
  language_detection::LanguageDetectionHost::CreateForWebContents(web_contents);
  observation_.Observe(
      language_detection::LanguageDetectionHost::FromWebContents(web_contents));
}

UrlLanguageHistogramRecorder::~UrlLanguageHistogramRecorder() = default;

void UrlLanguageHistogramRecorder::OnLanguageDetermined(
    const language_detection::LanguageDetectionDetails& details) {
  // If we have a language histogram (i.e. we're not in incognito), update it
  // with the detected language of every page visited.
  if (language_histogram_ && details.is_model_reliable) {
    language_histogram_->OnPageVisited(details.model_detected_language);
  }
}

void UrlLanguageHistogramRecorder::OnLanguageDetectionDriverDestroyed(
    language_detection::LanguageDetectionDriver* driver) {
  observation_.Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UrlLanguageHistogramRecorder);
