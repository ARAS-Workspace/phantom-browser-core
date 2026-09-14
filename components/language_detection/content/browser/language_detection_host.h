// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_HOST_H_
#define COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_HOST_H_

#include <optional>

#include "components/language_detection/content/common/language_detection.mojom.h"
#include "components/language_detection/core/language_detection_details.h"
#include "components/language_detection/core/language_detection_driver.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {
class WebContents;
}  // namespace content

namespace language_detection {

// Receives the detected language of one tab from its renderer and passes it to
// the language detection observers of that tab. One instance per WebContents.
class LanguageDetectionHost
    : public content::WebContentsUserData<LanguageDetectionHost>,
      public LanguageDetectionDriver,
      public mojom::LanguageDetectionHost {
 public:
  LanguageDetectionHost(const LanguageDetectionHost&) = delete;
  LanguageDetectionHost& operator=(const LanguageDetectionHost&) = delete;

  ~LanguageDetectionHost() override;

  // Adds a receiver in `receivers_` for the passed `receiver`.
  void AddReceiver(
      mojo::PendingReceiver<mojom::LanguageDetectionHost> receiver);

  // mojom::LanguageDetectionHost implementation:
  void LanguageDetermined(const LanguageDetectionDetails& details) override;

  // Passes `details` to every observer registered on this tab.
  void NotifyLanguageDetermined(const LanguageDetectionDetails& details);

  // The language most recently detected on this tab, if any.
  const std::optional<LanguageDetectionDetails>& last_details() const {
    return last_details_;
  }

 private:
  friend class content::WebContentsUserData<LanguageDetectionHost>;

  explicit LanguageDetectionHost(content::WebContents* web_contents);

  // One LanguageDetectionHost serves a whole WebContents, and a main frame is
  // replaced on some navigations, so more than one receiver can be live.
  mojo::ReceiverSet<mojom::LanguageDetectionHost> receivers_;

  std::optional<LanguageDetectionDetails> last_details_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_HOST_H_
