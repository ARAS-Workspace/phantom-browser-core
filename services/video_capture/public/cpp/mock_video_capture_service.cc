// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_video_capture_service.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace video_capture {

MockVideoCaptureService::MockVideoCaptureService() {}

MockVideoCaptureService::~MockVideoCaptureService() = default;

void MockVideoCaptureService::ConnectToVideoSourceProvider(
    mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver) {
  DoConnectToVideoSourceProvider(std::move(receiver));
}

}  // namespace video_capture
