// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/surface_utils.h"

#include "build/build_config.h"
#include "components/viz/host/host_frame_sink_manager.h"

#include "content/browser/compositor/image_transport_factory.h"
#include "ui/compositor/compositor.h"  // nogncheck

namespace content {

viz::FrameSinkId AllocateFrameSinkId() {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  return factory->GetContextFactory()->AllocateFrameSinkId();
}

viz::HostFrameSinkManager* GetHostFrameSinkManager() {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  if (!factory)
    return nullptr;
  return factory->GetContextFactory()->GetHostFrameSinkManager();
}

CopyFromSurfaceResult ToCopyFromSurfaceResult(
    base::expected<viz::CopyOutputBitmapWithMetadata,
                   viz::CopyOutputResult::Error> result) {
  if (!result.has_value()) {
    switch (result.error()) {
      case viz::CopyOutputResult::Error::kNone:
        return base::unexpected<CopyFromSurfaceError>(
            CopyFromSurfaceError::kVizSentEmptyBitmap);
      case viz::CopyOutputResult::Error::kUnknown:
        return base::unexpected<CopyFromSurfaceError>(
            CopyFromSurfaceError::kUnknownVizError);
      case viz::CopyOutputResult::Error::kTimeout:
        return base::unexpected<CopyFromSurfaceError>(
            CopyFromSurfaceError::kTimeout);
      case viz::CopyOutputResult::Error::kEmbeddingTokenChanged:
        return base::unexpected<CopyFromSurfaceError>(
            CopyFromSurfaceError::kEmbeddingTokenChanged);
    }
  }
  return std::move(result.value());
}

}  // namespace content
