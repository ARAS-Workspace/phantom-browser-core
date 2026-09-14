// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_HOST_H_
#define COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_HOST_H_

#include <optional>
#include <string>

#include "components/language_detection/content/common/language_detection.mojom.h"
#include "components/language_detection/core/language_detection_details.h"
#include "components/language_detection/core/language_detection_driver.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace language_detection {

// Receives the detected language of one tab from its renderer and passes it to
// the language detection observers of that tab. One instance per WebContents.
class LanguageDetectionHost
    : public content::WebContentsUserData<LanguageDetectionHost>,
      public content::WebContentsObserver,
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

  // The language detected on the page this tab shows now, or the empty string
  // while no language has been detected on it.
  std::string adopted_language() const;

  // The full result of that detection, if there is one.
  const std::optional<LanguageDetectionDetails>& last_details() const {
    return last_details_;
  }

  // content::WebContentsObserver implementation:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<LanguageDetectionHost>;

  explicit LanguageDetectionHost(content::WebContents* web_contents);

  // One LanguageDetectionHost serves a whole WebContents, and a main frame is
  // replaced on some navigations, so more than one receiver can be live.
  mojo::ReceiverSet<mojom::LanguageDetectionHost> receivers_;

  std::optional<LanguageDetectionDetails> last_details_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Returns the language detected on the page `web_contents` shows now, or the
// empty string when there is none. A tab without a LanguageDetectionHost reads
// as a tab whose language is unknown; this creates no host.
std::string GetAdoptedLanguage(content::WebContents* web_contents);

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_HOST_H_
