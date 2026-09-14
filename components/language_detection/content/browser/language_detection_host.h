// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_HOST_H_
#define COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_HOST_H_

#include "components/language_detection/core/language_detection_driver.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace language_detection {

struct LanguageDetectionDetails;

// Holds the language detection observers of one tab and passes the detected
// language to them. One instance per WebContents.
class LanguageDetectionHost
    : public content::WebContentsUserData<LanguageDetectionHost>,
      public LanguageDetectionDriver {
 public:
  LanguageDetectionHost(const LanguageDetectionHost&) = delete;
  LanguageDetectionHost& operator=(const LanguageDetectionHost&) = delete;

  ~LanguageDetectionHost() override;

  // Passes `details` to every observer registered on this tab.
  void NotifyLanguageDetermined(const LanguageDetectionDetails& details);

 private:
  friend class content::WebContentsUserData<LanguageDetectionHost>;

  explicit LanguageDetectionHost(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_HOST_H_
