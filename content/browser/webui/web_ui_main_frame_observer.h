// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_MAIN_FRAME_OBSERVER_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_MAIN_FRAME_OBSERVER_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace blink::mojom {
enum class ConsoleMessageLevel;
}  // namespace blink::mojom

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
class WebUIImpl;

// The WebContentObserver for WebUIImpl. Each WebUIImpl has exactly one
// WebUIMainFrameObserver to watch for notifications from the associated
// WebContents object.
class CONTENT_EXPORT WebUIMainFrameObserver : public WebContentsObserver {
 public:
  WebUIMainFrameObserver(WebUIImpl* web_ui, WebContents* contents);
  ~WebUIMainFrameObserver() override;
  WebUIMainFrameObserver(const WebUIMainFrameObserver& rhs) = delete;
  WebUIMainFrameObserver& operator=(const WebUIMainFrameObserver& rhs) = delete;

 protected:
  // Override from WebContentsObserver
  void PrimaryPageChanged(Page& page) override;

  // In development builds, crash the browser on the JavaScript errors of the
  // WebUI surfaces whose config asks for it.
  void OnDidAddMessageToConsole(
      RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) override;

  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;

 private:
  void MaybeEnableCrashOnJavaScriptError(NavigationHandle* navigation_handle);

  // Do we crash the browser on JavaScript errors?
  bool should_crash_on_error_ = false;

  raw_ptr<WebUIImpl> web_ui_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_MAIN_FRAME_OBSERVER_H_
