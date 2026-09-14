// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/content/renderer/language_detection_agent.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/language_detection/content/renderer/language_detection_model_manager.h"
#include "components/language_detection/core/constants.h"
#include "components/language_detection/core/features.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_language_detection_details.h"
#include "third_party/blink/public/web/web_local_frame.h"

using blink::WebDocument;
using blink::WebLanguageDetectionDetails;
using blink::WebLocalFrame;

namespace {

// The current CLD model version.
constexpr char kCLDModelVersion[] = "CLD3";

// Returns the translate model wrapper that is shared across the RenderFrames
// in the renderer. Named apart from language_detection::GetLanguageDetectionModel
// so that unqualified lookup inside namespace language_detection finds this one.
translate::LanguageDetectionModel& GetSharedModelWrapper() {
  static base::NoDestructor<translate::LanguageDetectionModel> instance(
      language_detection::GetLanguageDetectionModel());
  return *instance;
}

// Returns if the language detection should be overridden so that a default
// result is returned immediately.
bool ShouldOverrideLanguageDetectionForTesting() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kOverrideLanguageDetection)) {
    return true;
  }
  return false;
}

// Returns true if the language detection should be skipped for |url|.
// Limit detection to URLs that only detect the language of the content if the
// page is potentially a candidate for translation. This should be strictly a
// subset of the conditions in TranslateService::IsTranslatableURL, however,
// due to layering they cannot be identical. Critically, this list should
// never filter anything that is eligible for translation. Under filtering is
// ok as the translate service will make the final call and only results in a
// slight overhead in running the model when unnecessary.
bool ShouldSkipLanguageDetection(const GURL& url) {
  return url.is_empty() || url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(content::kChromeDevToolsScheme) || url.IsAboutBlank();
}

}  // namespace

namespace language_detection {

LanguageDetectionAgent::LanguageDetectionAgent(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      waiting_for_first_foreground_(render_frame->IsHidden()),
      language_detection_model_(&GetSharedModelWrapper()),
      language_detection_model_manager_(
          language_detection_model_->tflite_model()) {
  if (!translate::IsTFLiteLanguageDetectionEnabled()) {
    return;
  }

  // If the language detection model is available, we do not
  // worry about requesting the model.
  if (language_detection_model_->IsAvailable()) {
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("LanguageDetection.TFLiteModel.WasModelRequestDeferred",
                        waiting_for_first_foreground_);

  // Ensure the render frame is visible, otherwise the browser-side
  // driver may not exist yet (https://crbug.com/1199397).
  if (!waiting_for_first_foreground_) {
    RequestModel();
  }
}

LanguageDetectionAgent::~LanguageDetectionAgent() = default;

void LanguageDetectionAgent::SetDetectionCallback(DetectionCallback callback) {
  detection_callback_ = std::move(callback);
}

void LanguageDetectionAgent::SeedLanguageDetectionModelForTesting(
    base::File model_file) {
  language_detection_model_->tflite_model().UpdateWithFile(
      std::move(model_file));
}

void LanguageDetectionAgent::PrepareForNewDocument() {
  last_details_.reset();
}

void LanguageDetectionAgent::PageCaptured(
    scoped_refptr<const base::RefCountedString16> contents) {
  TRACE_EVENT("browser", "LanguageDetectionAgent::PageCaptured");
  // Get the document language as set by WebKit from the http-equiv
  // meta tag for "content-language".  This may or may not also
  // have a value derived from the actual Content-Language HTTP
  // header.  The two actually have different meanings (despite the
  // original intent of http-equiv to be an equivalent) with the former
  // being the language of the document and the latter being the
  // language of the intended audience (a distinction really only
  // relevant for things like language textbooks). This distinction
  // shouldn't affect translation.
  if (!contents) {
    return;
  }
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame) {
    return;
  }

  blink::WebDocumentLoader* doc_loader = main_frame->GetDocumentLoader();
  if (doc_loader &&
      doc_loader->GetWebResponse().MimeType() == "application/pdf") {
    // If the page is a PDF, we should only register it when
    // PdfPageCaptured is called.
    return;
  }

  WebDocument document = main_frame->GetDocument();
  GURL url = GURL(document.Url());
  if (ShouldSkipLanguageDetection(url)) {
    LanguageDetectionDetails details;
    details.time = base::Time::Now();
    details.url = url;
    details.has_run_lang_detection = false;
    Report(std::move(details),
           /*page_level_translation_criteria_met=*/false);
    return;
  }

  WebLanguageDetectionDetails web_detection_details =
      WebLanguageDetectionDetails::CollectLanguageDetectionDetails(document);
  WebLanguageDetectionDetails::RecordAcceptLanguageAndXmlHtmlLangMetric(
      document);

  std::string content_language = web_detection_details.content_language.Utf8();
  std::string html_lang = web_detection_details.html_language.Utf8();

  RunLanguageDetection(contents->as_string(), content_language, html_lang, url,
                       web_detection_details.has_no_translate_meta);
}

void LanguageDetectionAgent::PdfPageCaptured(const std::u16string& contents,
                                             const std::string& pdf_lang,
                                             const GURL& url) {
  TRACE_EVENT("browser", "LanguageDetectionAgent::PdfPageCaptured");
  if (ShouldSkipLanguageDetection(url)) {
    return;
  }

  // PDF uses the document language metadata as both content language and
  // HTML language for language detection purposes.
  // PDF does not support the "notranslate" meta tag.
  RunLanguageDetection(contents, pdf_lang, pdf_lang, url,
                       /*has_notranslate=*/false);
}

void LanguageDetectionAgent::RunLanguageDetection(
    const std::u16string& contents,
    const std::string& content_language,
    const std::string& html_lang,
    const GURL& url,
    bool has_notranslate) {
  page_contents_length_ = contents.size();

  if (ShouldOverrideLanguageDetectionForTesting()) {
    LanguageDetectionDetails details;
    details.adopted_language = "fr";
    details.contents = contents;
    details.has_run_lang_detection = true;

    bool criteria =
        !details.has_notranslate && !details.adopted_language.empty();
    Report(std::move(details), criteria);
    return;
  }

  std::string model_detected_language;
  bool is_model_reliable = false;
  std::string detection_model_version;
  float model_reliability_score = 0.0;

  LanguageDetectionDetails details;
  std::string language;
  if (page_contents_length_ == 0) {
    // If captured content is empty do not run language detection and
    // only use page-provided languages.
    language = translate::DeterminePageLanguageNoModel(
        content_language, html_lang,
        translate::LanguageVerificationType::kNoPageContent);
  } else if (translate::IsTFLiteLanguageDetectionEnabled()) {
    // Use TFLite and page contents to assist with language detection.
    bool is_available = language_detection_model_->IsAvailable();
    language =
        is_available
            ? language_detection_model_->DeterminePageLanguage(
                  content_language, html_lang, contents,
                  &model_detected_language, &is_model_reliable,
                  model_reliability_score)
            // If the model is not available do not run language
            // detection and only use page-provided languages.
            : translate::DeterminePageLanguageNoModel(
                  content_language, html_lang,
                  translate::LanguageVerificationType::kModelNotAvailable);
    UMA_HISTOGRAM_BOOLEAN(
        "LanguageDetection.TFLiteModel.WasModelAvailableForDetection",
        is_available);
    UMA_HISTOGRAM_BOOLEAN(
        "LanguageDetection.TFLiteModel.WasModelUnavailableDueToDeferredLoad",
        !is_available && waiting_for_first_foreground_);
    detection_model_version = language_detection_model_->GetModelVersion();
    details.has_run_lang_detection = true;
  } else {
    // Use CLD3 and page contents to assist with language detection.
    language = translate::DeterminePageLanguage(
        content_language, html_lang, contents, &model_detected_language,
        &is_model_reliable, model_reliability_score);
    detection_model_version = kCLDModelVersion;
    details.has_run_lang_detection = true;
  }

  details.time = base::Time::Now();
  details.url = url;
  details.content_language = content_language;
  details.model_detected_language = model_detected_language;
  details.is_model_reliable = is_model_reliable;
  details.has_notranslate = has_notranslate;
  details.html_root_language = html_lang;
  details.adopted_language = language;
  details.model_reliability_score = model_reliability_score;
  details.detection_model_version = detection_model_version;

  // TODO(hajimehoshi): If this affects performance, it should be set only if
  // translate-internals tab exists.
  details.contents = contents;

  bool criteria = !details.has_notranslate && !details.adopted_language.empty();
  Report(std::move(details), criteria);
}

void LanguageDetectionAgent::Report(LanguageDetectionDetails details,
                                    bool page_level_translation_criteria_met) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame) {
    return;
  }

  GetHost()->LanguageDetermined(details);

  if (detection_callback_) {
    detection_callback_.Run(details, page_level_translation_criteria_met);
  }

  last_details_ = std::move(details);
}

void LanguageDetectionAgent::RenewPageRegistration() {
  if (!last_details_.has_value()) {
    return;
  }

  LanguageDetectionDetails details = std::move(*last_details_);
  bool criteria = !details.has_notranslate && !details.adopted_language.empty();
  Report(std::move(details), criteria);
}

const mojo::Remote<mojom::LanguageDetectionHost>&
LanguageDetectionAgent::GetHost() {
  if (host_) {
    if (host_.is_connected()) {
      return host_;
    }
    // The host can become unbound or disconnected in testing so this catches
    // that case and reconnects so `this` can connect to the browser.
    host_.reset();
  }

  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      host_.BindNewPipeAndPassReceiver());
  return host_;
}

void LanguageDetectionAgent::WasShown() {
  // Check if the the render frame was initially hidden and
  // the model request was delayed until the frame was in
  // the foreground.
  if (!waiting_for_first_foreground_) {
    return;
  }

  waiting_for_first_foreground_ = false;

  if (!translate::IsTFLiteLanguageDetectionEnabled()) {
    return;
  }

  if (language_detection_model_->IsAvailable()) {
    return;
  }
  // The model request was deferred because the frame was hidden
  // and now the model is visible and the model is still not available.
  // The browser-side translate driver should always be available at
  // this point so we should make the request and race to get the
  // model loaded for when the page content is available.
  RequestModel();
}

void LanguageDetectionAgent::OnDestruct() {
  delete this;
}

void LanguageDetectionAgent::RequestModel() {
  language_detection_model_manager_.GetLanguageDetectionModel(
      render_frame()->GetBrowserInterfaceBroker(),
      base::BindOnce([](LanguageDetectionModel* model) {}));
}

}  // namespace language_detection
