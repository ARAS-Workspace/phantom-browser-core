// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/content/browser/language_detection_frame_binder.h"

#include "components/language_detection/content/browser/language_detection_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace language_detection {

void BindLanguageDetectionHost(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::LanguageDetectionHost> receiver) {
  // Only valid for the primary main frame.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return;
  }

  LanguageDetectionHost::CreateForWebContents(web_contents);
  LanguageDetectionHost::FromWebContents(web_contents)
      ->AddReceiver(std::move(receiver));
}

}  // namespace language_detection
