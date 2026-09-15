// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_AGENT_H_
#define COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_AGENT_H_

#include <optional>
#include <string>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "components/language_detection/content/common/language_detection.mojom.h"
#include "components/language_detection/content/renderer/language_detection_model_manager.h"
#include "components/language_detection/core/language_detection_details.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace language_detection {

class PageLanguageDetector;

// Runs language detection on the content of a main frame and reports the
// result to the browser over mojom::LanguageDetectionHost. There is one
// LanguageDetectionAgent per main frame.
class LanguageDetectionAgent : public content::RenderFrameObserver {
 public:
  // An extra sink for a detection result, invoked with the same details that
  // are sent to the browser.
  using DetectionCallback =
      base::RepeatingCallback<void(const LanguageDetectionDetails&, bool)>;

  explicit LanguageDetectionAgent(content::RenderFrame* render_frame);

  LanguageDetectionAgent(const LanguageDetectionAgent&) = delete;
  LanguageDetectionAgent& operator=(const LanguageDetectionAgent&) = delete;

  ~LanguageDetectionAgent() override;

  // Navigated to a new document.
  void PrepareForNewDocument();

  // Informs us that the page's text has been extracted.
  void PageCaptured(scoped_refptr<const base::RefCountedString16> contents);

  // Informs us that a PDF's text has been extracted.
  void PdfPageCaptured(const std::u16string& contents,
                       const std::string& pdf_lang,
                       const GURL& url);

  // Reports the last detection result again.
  void RenewPageRegistration();

  // Adds a sink for detection results.
  void SetDetectionCallback(DetectionCallback callback);

  // Set the language detection model used by |this|. For testing only.
  void SeedLanguageDetectionModelForTesting(base::File model_file);

  bool waiting_for_first_foreground() { return waiting_for_first_foreground_; }

  // The page content length at language detection time.
  size_t page_contents_length() const { return page_contents_length_; }

 private:
  // content::RenderFrameObserver implementation.
  void WasShown() override;
  void OnDestruct() override;

  void RequestModel();

  // Runs language detection on the provided contents and reports the result.
  void RunLanguageDetection(const std::u16string& contents,
                            const std::string& content_language,
                            const std::string& html_lang,
                            const GURL& url,
                            bool has_notranslate);

  // Sends |details| to the browser and to the detection callback.
  void Report(LanguageDetectionDetails details,
              bool page_level_translation_criteria_met);

  const mojo::Remote<mojom::LanguageDetectionHost>& GetHost();

  // Whether the render frame observed by |this| was initially hidden and
  // the request for a model is delayed until the frame is in the foreground.
  bool waiting_for_first_foreground_;

  // The page content length at language detection time.
  size_t page_contents_length_ = 0;

  // The shared model wrapper. Not owned by `this`. It outlives `this`.
  const raw_ptr<PageLanguageDetector> language_detection_model_;

  LanguageDetectionModelManager language_detection_model_manager_;

  // The Mojo pipe used to report detection results to the browser.
  mojo::Remote<mojom::LanguageDetectionHost> host_;

  // An extra sink for detection results.
  DetectionCallback detection_callback_;

  // The last reported detection result.
  std::optional<LanguageDetectionDetails> last_details_;

  // Weak pointer factory used to provide references to the detection host.
  base::WeakPtrFactory<LanguageDetectionAgent> weak_pointer_factory_{this};
};

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_AGENT_H_
