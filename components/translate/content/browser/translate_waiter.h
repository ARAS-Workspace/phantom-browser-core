// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_WAITER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_WAITER_H_

#include <memory>
#include <string_view>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/language_detection/core/language_detection_driver.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/common/translate_errors.h"

namespace translate {

// A helper class that allows test to block until certain translate events have
// been received from a TranslateDriver.
class TranslateWaiter : language_detection::LanguageDetectionDriver::Observer,
                        ContentTranslateDriver::TranslationObserver {
 public:
  enum class WaitEvent {
    kLanguageDetermined,
    kPageTranslated,
    kIsPageTranslatedChanged
  };

  TranslateWaiter(ContentTranslateDriver* translate_driver,
                  WaitEvent wait_event);

  TranslateWaiter(const TranslateWaiter&) = delete;
  TranslateWaiter& operator=(const TranslateWaiter&) = delete;

  ~TranslateWaiter() override;

  // Blocks until an observer function matching |wait_event_| is invoked, or
  // returns immediately if one has already been observed.
  void Wait();

  // language_detection::LanguageDetectionDriver::Observer:
  void OnLanguageDetermined(
      const language_detection::LanguageDetectionDetails& details) override;

  // ContentTranslateDriver::TranslationObserver:
  void OnPageTranslated(std::string_view source_lang,
                        std::string_view translated_lang,
                        TranslateErrors error_type) override;
  void OnIsPageTranslatedChanged(content::WebContents* source) override;

 private:
  WaitEvent wait_event_;
  base::ScopedObservation<language_detection::LanguageDetectionDriver,
                          language_detection::LanguageDetectionDriver::Observer>
      scoped_language_detection_observation_{this};
  base::ScopedObservation<ContentTranslateDriver,
                          ContentTranslateDriver::TranslationObserver>
      scoped_translation_observation_{this};
  base::RunLoop run_loop_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_TRANSLATE_WAITER_H_
