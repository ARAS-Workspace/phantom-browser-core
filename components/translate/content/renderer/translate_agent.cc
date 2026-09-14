// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/renderer/translate_agent.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/json/string_escape.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/language_detection/content/renderer/language_detection_agent.h"
#include "components/language_detection/core/constants.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "components/translate/content/renderer/isolated_world_util.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_language_detection_details.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8.h"

using blink::WebDocument;
using blink::WebLocalFrame;
using blink::WebScriptSource;
using blink::WebString;

namespace {

// The delay in milliseconds that we'll wait before checking to see if the
// translate library injected in the page is ready.
const int kTranslateInitCheckDelayMs = 150;

// The maximum number of times we'll check to see if the translate library
// injected in the page is ready.
const int kMaxTranslateInitCheckAttempts = 5;

// The delay we wait in milliseconds before checking whether the translation has
// finished.
const int kTranslateStatusCheckDelayMs = 400;

// Language name passed to the Translate element for it to detect the language.
const char kAutoDetectionLanguage[] = "auto";

}  // namespace

namespace translate {

////////////////////////////////////////////////////////////////////////////////
// TranslateAgent, public:
TranslateAgent::TranslateAgent(
    content::RenderFrame* render_frame,
    int world_id,
    language_detection::LanguageDetectionAgent* language_detection_agent)
    : content::RenderFrameObserver(render_frame),
      world_id_(world_id),
      language_detection_agent_(language_detection_agent) {
  translate_task_runner_ = this->render_frame()->GetTaskRunner(
      blink::TaskType::kInternalTranslation);
  if (language_detection_agent_) {
    language_detection_agent_->SetDetectionCallback(
        base::BindRepeating(&TranslateAgent::OnLanguageDetermined,
                            weak_pointer_factory_.GetWeakPtr()));
  }
}

TranslateAgent::~TranslateAgent() = default;

void TranslateAgent::PrepareForNewDocument() {
  // Navigated to a new document, reset current page translation.
  ResetPage();
}

void TranslateAgent::OnLanguageDetermined(
    const language_detection::LanguageDetectionDetails& details,
    bool page_level_translation_criteria_met) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame) {
    return;
  }

  // For the same render frame with the same url, each time when its texts are
  // captured, it should be treated as a new page to do translation.
  ResetPage();
  GetTranslateHandler()->RegisterPage(
      receiver_.BindNewPipeAndPassRemote(
          main_frame->GetTaskRunner(blink::TaskType::kInternalTranslation)),
      details, page_level_translation_criteria_met);
}

void TranslateAgent::CancelPendingTranslation() {
  weak_method_factory_.InvalidateWeakPtrs();
  // Make sure to send the cancelled response back.
  if (translate_callback_pending_) {
    std::move(translate_callback_pending_)
        .Run(true, source_lang_, target_lang_, TranslateErrors::NONE);
  }
  source_lang_.clear();
  target_lang_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// TranslateAgent, protected:
bool TranslateAgent::IsTranslateLibAvailable() {
  return ExecuteScriptAndGetBoolResult(
      "typeof cr != 'undefined' && typeof cr.googleTranslate != 'undefined' && "
      "typeof cr.googleTranslate.translate == 'function'",
      false);
}

bool TranslateAgent::IsTranslateLibReady() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.libReady", false);
}

bool TranslateAgent::HasTranslationFinished() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.finished", true);
}

bool TranslateAgent::HasTranslationFailed() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.error", true);
}

int64_t TranslateAgent::GetErrorCode() {
  int64_t error_code =
      ExecuteScriptAndGetIntegerResult("cr.googleTranslate.errorCode");
  DCHECK_LT(error_code, static_cast<int>(TranslateErrors::TRANSLATE_ERROR_MAX));
  return error_code;
}

bool TranslateAgent::StartTranslation() {
  return ExecuteScriptAndGetBoolResult(
      BuildTranslationScript(source_lang_, target_lang_), false);
}

std::string TranslateAgent::GetPageSourceLanguage() {
  return ExecuteScriptAndGetStringResult("cr.googleTranslate.sourceLang");
}

base::TimeDelta TranslateAgent::AdjustDelay(int delay_in_milliseconds) {
  // Just converts |delay_in_milliseconds| without any modification in practical
  // cases. Tests will override this function to return modified value.
  return base::Milliseconds(delay_in_milliseconds);
}

void TranslateAgent::ExecuteScript(const std::string& script) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return;

  WebScriptSource source = WebScriptSource(WebString::FromAscii(script));
  main_frame->ExecuteScriptInIsolatedWorld(
      world_id_, source, blink::BackForwardCacheAware::kAllow);
}

bool TranslateAgent::ExecuteScriptAndGetBoolResult(const std::string& script,
                                                   bool fallback) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return fallback;

  v8::HandleScope handle_scope(main_frame->GetAgentGroupScheduler()->Isolate());
  WebScriptSource source = WebScriptSource(WebString::FromAscii(script));
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_, source, blink::BackForwardCacheAware::kAllow);
  if (result.IsEmpty() || !result->IsBoolean()) {
    return fallback;
  }

  return result.As<v8::Boolean>()->Value();
}

std::string TranslateAgent::ExecuteScriptAndGetStringResult(
    const std::string& script) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return std::string();

  v8::Isolate* isolate = main_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  WebScriptSource source = WebScriptSource(WebString::FromAscii(script));
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_, source, blink::BackForwardCacheAware::kAllow);
  if (result.IsEmpty() || !result->IsString()) {
    return std::string();
  }

  v8::Local<v8::String> v8_str = result.As<v8::String>();
  size_t length = v8_str->Utf8LengthV2(isolate);
  if (length == 0) {
    return std::string();
  }

  std::string str(length, '\0');
  v8_str->WriteUtf8V2(isolate, str.data(), length);
  return str;
}

double TranslateAgent::ExecuteScriptAndGetDoubleResult(
    const std::string& script) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return 0.0;

  v8::HandleScope handle_scope(main_frame->GetAgentGroupScheduler()->Isolate());
  WebScriptSource source = WebScriptSource(WebString::FromAscii(script));
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_, source, blink::BackForwardCacheAware::kAllow);
  if (result.IsEmpty() || !result->IsNumber()) {
    return 0.0;
  }

  return result.As<v8::Number>()->Value();
}

int64_t TranslateAgent::ExecuteScriptAndGetIntegerResult(
    const std::string& script) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return 0;

  v8::HandleScope handle_scope(main_frame->GetAgentGroupScheduler()->Isolate());
  WebScriptSource source = WebScriptSource(WebString::FromAscii(script));
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id_, source, blink::BackForwardCacheAware::kAllow);
  if (result.IsEmpty() || !result->IsNumber()) {
    return 0;
  }

  return result.As<v8::Integer>()->Value();
}

// mojom::TranslateAgent implementations.
void TranslateAgent::TranslateFrame(const std::string& translate_script,
                                    const std::string& source_lang,
                                    const std::string& target_lang,
                                    TranslateFrameCallback callback) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame) {
    // Cancelled.
    std::move(callback).Run(true, source_lang, target_lang,
                            TranslateErrors::NONE);
    return;  // We navigated away, nothing to do.
  }

  // A similar translation is already under way, nothing to do.
  if (translate_callback_pending_ && target_lang_ == target_lang) {
    // This request is ignored.
    std::move(callback).Run(true, source_lang, target_lang,
                            TranslateErrors::NONE);
    return;
  }

  // Any pending translation is now irrelevant.
  CancelPendingTranslation();

  // Set our states.
  translate_callback_pending_ = std::move(callback);

  // If the source language is undetermined, we'll let the translate element
  // detect it.
  source_lang_ = (source_lang != language_detection::kUnknownLanguageCode)
                     ? source_lang
                     : kAutoDetectionLanguage;
  target_lang_ = target_lang;

  // Set up v8 isolated world.
  EnsureIsolatedWorldInitialized(world_id_);

  // Executing script may spin a nested run loop that detaches the frame and
  // deletes |this|.
  auto weak_this = weak_pointer_factory_.GetWeakPtr();
  if (!IsTranslateLibAvailable()) {
    if (!weak_this) {
      return;
    }
    // Evaluate the script to add the translation related method to the global
    // context of the page.
    ExecuteScript(translate_script);
    if (!weak_this) {
      return;
    }
    DCHECK(IsTranslateLibAvailable());
  }
  if (!weak_this) {
    return;
  }

  TranslatePageImpl(0);
}

void TranslateAgent::RevertTranslation() {
  // Executing script may spin a nested run loop that detaches the frame and
  // deletes |this|.
  auto weak_this = weak_pointer_factory_.GetWeakPtr();
  if (!IsTranslateLibAvailable()) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }
  if (!weak_this) {
    return;
  }

  CancelPendingTranslation();

  ExecuteScript("cr.googleTranslate.revert()");
}

////////////////////////////////////////////////////////////////////////////////
// TranslateAgent, private:
void TranslateAgent::CheckTranslateStatus() {
  // Executing script may spin a nested run loop that detaches the frame and
  // deletes |this|.
  auto weak_this = weak_pointer_factory_.GetWeakPtr();
  // First check if there was an error.
  if (HasTranslationFailed()) {
    if (!weak_this) {
      return;
    }
    TranslateErrors error =
        static_cast<translate::TranslateErrors>(GetErrorCode());
    if (!weak_this) {
      return;
    }
    NotifyBrowserTranslationFailed(error);
    return;  // There was an error.
  }
  if (!weak_this) {
    return;
  }

  if (HasTranslationFinished()) {
    if (!weak_this) {
      return;
    }
    std::string actual_source_lang;
    // Translation was successfull, if it was auto, retrieve the source
    // language the Translate Element detected.
    if (source_lang_ == kAutoDetectionLanguage) {
      actual_source_lang = GetPageSourceLanguage();
      if (!weak_this) {
        return;
      }
      if (actual_source_lang.empty()) {
        NotifyBrowserTranslationFailed(TranslateErrors::UNKNOWN_LANGUAGE);
        return;
      } else if (actual_source_lang == target_lang_) {
        NotifyBrowserTranslationFailed(TranslateErrors::IDENTICAL_LANGUAGES);
        return;
      }
    } else {
      actual_source_lang = source_lang_;
    }

    if (!translate_callback_pending_) {
      NOTREACHED();
    }

    // Check JavaScript performance counters for UMA reports.
    ReportTimeToTranslate(
        ExecuteScriptAndGetDoubleResult("cr.googleTranslate.translationTime"));
    if (!weak_this) {
      return;
    }
    ReportTranslatedLanguageDetectionContentLength(
        language_detection_agent_
            ? language_detection_agent_->page_contents_length()
            : 0u);

    // Notify the browser we are done.
    std::move(translate_callback_pending_)
        .Run(false, actual_source_lang, target_lang_, TranslateErrors::NONE);
    return;
  }
  if (!weak_this) {
    return;
  }

  // The translation is still pending, check again later.
  translate_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TranslateAgent::CheckTranslateStatus,
                     weak_method_factory_.GetWeakPtr()),
      AdjustDelay(kTranslateStatusCheckDelayMs));
}

void TranslateAgent::TranslatePageImpl(int count) {
  DCHECK_LT(count, kMaxTranslateInitCheckAttempts);
  // Executing script may spin a nested run loop that detaches the frame and
  // deletes |this|.
  auto weak_this = weak_pointer_factory_.GetWeakPtr();
  if (!IsTranslateLibReady()) {
    if (!weak_this) {
      return;
    }
    // There was an error during initialization of library.
    TranslateErrors error =
        static_cast<translate::TranslateErrors>(GetErrorCode());
    if (!weak_this) {
      return;
    }
    if (error != TranslateErrors::NONE) {
      NotifyBrowserTranslationFailed(error);
      return;
    }

    // The library is not ready, try again later, unless we have tried several
    // times unsuccessfully already.
    if (++count >= kMaxTranslateInitCheckAttempts) {
      NotifyBrowserTranslationFailed(TranslateErrors::TRANSLATION_TIMEOUT);
      return;
    }
    translate_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TranslateAgent::TranslatePageImpl,
                       weak_method_factory_.GetWeakPtr(), count),
        AdjustDelay(count * kTranslateInitCheckDelayMs));
    return;
  }
  if (!weak_this) {
    return;
  }

  // The library is loaded, and ready for translation now.
  // Check JavaScript performance counters for UMA reports.
  ReportTimeToBeReady(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.readyTime"));
  if (!weak_this) {
    return;
  }
  ReportTimeToLoad(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.loadTime"));
  if (!weak_this) {
    return;
  }

  if (!StartTranslation()) {
    if (!weak_this) {
      return;
    }
    CheckTranslateStatus();
    return;
  }
  if (!weak_this) {
    return;
  }
  // Check the status of the translation.
  translate_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TranslateAgent::CheckTranslateStatus,
                     weak_method_factory_.GetWeakPtr()),
      AdjustDelay(kTranslateStatusCheckDelayMs));
}

void TranslateAgent::NotifyBrowserTranslationFailed(TranslateErrors error) {
  DCHECK(translate_callback_pending_);
  // Notify the browser there was an error.
  std::move(translate_callback_pending_)
      .Run(false, source_lang_, target_lang_, error);
}

const mojo::Remote<mojom::ContentTranslateDriver>&
TranslateAgent::GetTranslateHandler() {
  if (translate_handler_) {
    if (translate_handler_.is_connected()) {
      return translate_handler_;
    }
    // The translate handler can become unbound or disconnected in testing
    // so this catches that case and reconnects so `this` can connect to
    // the driver in the browser.
    translate_handler_.reset();
  }

  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      translate_handler_.BindNewPipeAndPassReceiver());
  return translate_handler_;
}

void TranslateAgent::ResetPage() {
  receiver_.reset();
  translate_callback_pending_.Reset();
  CancelPendingTranslation();
}

void TranslateAgent::OnDestruct() {
  delete this;
}

/* static */
std::string TranslateAgent::BuildTranslationScript(
    const std::string& source_lang,
    const std::string& target_lang) {
  return "cr.googleTranslate.translate(" +
         base::GetQuotedJSONString(source_lang) + "," +
         base::GetQuotedJSONString(target_lang) + ")";
}

}  // namespace translate
