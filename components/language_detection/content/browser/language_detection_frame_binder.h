// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_FRAME_BINDER_H_
#define COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_FRAME_BINDER_H_

#include "components/language_detection/content/common/language_detection.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace language_detection {

// Binds `receiver` to the LanguageDetectionHost of the tab that
// `render_frame_host` belongs to. Only the primary main frame is served.
void BindLanguageDetectionHost(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::LanguageDetectionHost> receiver);

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_LANGUAGE_DETECTION_FRAME_BINDER_H_
