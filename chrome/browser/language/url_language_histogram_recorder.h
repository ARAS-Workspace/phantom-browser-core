// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_URL_LANGUAGE_HISTOGRAM_RECORDER_H_
#define CHROME_BROWSER_LANGUAGE_URL_LANGUAGE_HISTOGRAM_RECORDER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/language_detection/core/language_detection_driver.h"
#include "content/public/browser/web_contents_user_data.h"

namespace language {
class UrlLanguageHistogram;
}  // namespace language

namespace content {
class WebContents;
}  // namespace content

// Feeds the detected language of every page visited in one tab to the profile
// wide language::UrlLanguageHistogram. One instance per WebContents.
class UrlLanguageHistogramRecorder
    : public content::WebContentsUserData<UrlLanguageHistogramRecorder>,
      public language_detection::LanguageDetectionDriver::Observer {
 public:
  UrlLanguageHistogramRecorder(const UrlLanguageHistogramRecorder&) = delete;
  UrlLanguageHistogramRecorder& operator=(const UrlLanguageHistogramRecorder&) =
      delete;

  ~UrlLanguageHistogramRecorder() override;

  // language_detection::LanguageDetectionDriver::Observer implementation:
  void OnLanguageDetermined(
      const language_detection::LanguageDetectionDetails& details) override;
  void OnLanguageDetectionDriverDestroyed(
      language_detection::LanguageDetectionDriver* driver) override;

 private:
  friend class content::WebContentsUserData<UrlLanguageHistogramRecorder>;

  explicit UrlLanguageHistogramRecorder(content::WebContents* web_contents);

  // Null in incognito.
  const raw_ptr<language::UrlLanguageHistogram> language_histogram_;

  base::ScopedObservation<
      language_detection::LanguageDetectionDriver,
      language_detection::LanguageDetectionDriver::Observer>
      observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_LANGUAGE_URL_LANGUAGE_HISTOGRAM_RECORDER_H_
