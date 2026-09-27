// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_

#include <optional>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"


namespace content {
class Page;

// An interface consisting of methods that can be called to use Message ports.
class CONTENT_EXPORT MessagePortProvider {
 public:
  MessagePortProvider() = delete;
  MessagePortProvider(const MessagePortProvider&) = delete;
  MessagePortProvider& operator=(const MessagePortProvider&) = delete;

  // Posts a MessageEvent to the main frame using the given source and target
  // origins and data.
  // See https://html.spec.whatwg.org/multipage/comms.html#messageevent for
  // further information on message events.
  // Should be called on UI thread.
  static void PostMessageToFrame(Page& page,
                                 const url::Origin* source_origin,
                                 const url::Origin* target_origin,
                                 const blink::WebMessagePayload& data);

// Fuchsia WebEngine always uses this version.
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_
