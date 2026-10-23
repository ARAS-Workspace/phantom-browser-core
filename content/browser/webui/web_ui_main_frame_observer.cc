// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_main_frame_observer.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version_info/version_info.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace content {

namespace {

// The command line flag to disable the crash on WebUI JS error in development
// builds.
const char kDisableCrashOnWebUIJsError[] = "disable-crash-on-webui-js-error";

// Returns `url` without its username, password, query and fragment, for the
// crash message.
std::string RedactUrl(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements).spec();
}

}  // namespace

WebUIMainFrameObserver::WebUIMainFrameObserver(WebUIImpl* web_ui,
                                               WebContents* contents)
    : WebContentsObserver(contents), web_ui_(web_ui) {}

WebUIMainFrameObserver::~WebUIMainFrameObserver() = default;

void WebUIMainFrameObserver::OnDidAddMessageToConsole(
    RenderFrameHost* source_frame,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id,
    const std::optional<std::u16string>& untrusted_stack_trace) {
  DVLOG(3) << "OnDidAddMessageToConsole called for " << message;
  if (untrusted_stack_trace) {
    DVLOG(3) << "stack is " << *untrusted_stack_trace;
  }

  if (!should_crash_on_error_) {
    DVLOG(3) << "Message ignored, no crash on error for this page";
    return;
  }

  if (log_level != blink::mojom::ConsoleMessageLevel::kError) {
    DVLOG(3) << "Message ignored, not an error";
    return;
  }

  // Some WebUI pages have another WebUI page in an <iframe>. Both
  // WebUIMainFrameObservers will get a callback when either page gets an error.
  // To avoid duplicates, only handle errors from this page's frame.
  if (source_frame != web_ui_->GetRenderFrameHost()) {
    DVLOG(3) << "Message ignored, different frame";
    return;
  }

  GURL url(source_id);
  if (!url.is_valid()) {
    DVLOG(3) << "Message ignored, invalid URL";
    return;
  }

  // If this is not a chrome:// page, ignore the error. In particular, some
  // WebUIs use chrome-untrusted:// to host pages with some
  // not-controlled-by-Chrome content, whose errors must not crash the browser.
  if (!url.SchemeIs(kChromeUIScheme)) {
    DVLOG(3) << "Message ignored, not a chrome:// URL";
    return;
  }

  // Ignore console messages from frames that are pending deletion to avoid
  // crashing on errors during frame teardown.
  if (source_frame->GetLifecycleState() ==
      RenderFrameHost::LifecycleState::kPendingDeletion) {
    DVLOG(3) << "Message ignored, frame is being deleted";
    return;
  }

  std::string stack_trace = "(empty)";
  if (untrusted_stack_trace) {
    stack_trace = base::UTF16ToUTF8(*untrusted_stack_trace);
  }

  std::string redacted_page_url = "(empty)";
  GURL page_url = source_frame->GetLastCommittedURL();
  if (page_url.is_valid()) {
    redacted_page_url = RedactUrl(page_url);
  }

  // LOG(FATAL) will crash the browser.
  LOG(FATAL) << "JavaScript error in development build. To disable this "
                "crash, use --"
             << kDisableCrashOnWebUIJsError << ".\n"
             << "URL: " << RedactUrl(url) << "\n"
             << "Page URL: " << redacted_page_url << "\n"
             << "Message: " << base::UTF16ToUTF8(message) << "\n"
             << "Stack trace: " << stack_trace;
}

void WebUIMainFrameObserver::MaybeEnableCrashOnJavaScriptError(
    NavigationHandle* navigation_handle) {
  // WebUIConfigMap::GetConfig() uses the origin of the URL to look up the
  // config, so this works even if the page URL has a path, e.g.
  // chrome://omnibox-popup.top-chrome/omnibox_popup_aim.html.
  WebUIConfig* config = WebUIConfigMap::GetInstance().GetConfig(
      web_contents()->GetBrowserContext(), navigation_handle->GetURL());
  should_crash_on_error_ =
      config && config->ShouldCrashOnJavascriptErrorInDevelopmentBuild() &&
      !version_info::IsOfficialBuild();

  // Allow disabling the crash via a command line switch, for testing purposes.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableCrashOnWebUIJsError)) {
    should_crash_on_error_ = false;
  }

  // If we crash on JavaScript errors, make sure the main frame sends us stacks
  // along with those messages. Warning: Don't call
  // RenderFrameHostImpl::SetWantErrorMessageStackTrace() before the remote
  // frame is created, or it will lock up the communications channel. (See
  // https://crbug.com/1154866).
  if (should_crash_on_error_) {
    DVLOG(3) << "Enabled";
    static_cast<content::RenderFrameHostImpl*>(web_ui_->GetRenderFrameHost())
        ->SetWantErrorMessageStackTrace();
  } else {
    DVLOG(3) << "Crash on JavaScript error disabled for this page";
  }
}

void WebUIMainFrameObserver::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  // Navigation didn't occur in the frame associated with this WebUI.
  if (navigation_handle->GetRenderFrameHost() !=
      web_ui_->GetRenderFrameHost()) {
    return;
  }

  web_ui_->SetUpMojoInterfaceBroker();

  GetContentClient()->browser()->LogWebUIUsage(web_ui_);
  MaybeEnableCrashOnJavaScriptError(navigation_handle);
}

void WebUIMainFrameObserver::PrimaryPageChanged(Page& page) {
  web_ui_->DisallowJavascriptOnAllHandlers();
  web_ui_->GetController()->WebUIPrimaryPageChanged(page);
}

}  // namespace content
