// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/websocket_handshake_throttle_provider_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"

WebSocketHandshakeThrottleProviderImpl::WebSocketHandshakeThrottleProviderImpl(
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker) {
  DETACH_FROM_THREAD(thread_checker_);
}

WebSocketHandshakeThrottleProviderImpl::
    ~WebSocketHandshakeThrottleProviderImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

WebSocketHandshakeThrottleProviderImpl::WebSocketHandshakeThrottleProviderImpl(
    const WebSocketHandshakeThrottleProviderImpl& other) {
  DETACH_FROM_THREAD(thread_checker_);
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
WebSocketHandshakeThrottleProviderImpl::Clone(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return base::WrapUnique(new WebSocketHandshakeThrottleProviderImpl(*this));
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
WebSocketHandshakeThrottleProviderImpl::CreateThrottle(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return nullptr;
}
