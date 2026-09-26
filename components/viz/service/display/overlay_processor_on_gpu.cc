// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_on_gpu.h"

#include "build/build_config.h"
#include "gpu/command_buffer/service/display_compositor_memory_and_task_controller_on_gpu.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

OverlayProcessorOnGpu::OverlayProcessorOnGpu(
    gpu::DisplayCompositorMemoryAndTaskControllerOnGpu*
        display_controller_on_gpu)
    : shared_image_representation_factory_(
          std::make_unique<gpu::SharedImageRepresentationFactory>(
              display_controller_on_gpu->shared_image_manager(),
              display_controller_on_gpu->memory_tracker())) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

OverlayProcessorOnGpu::~OverlayProcessorOnGpu() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void OverlayProcessorOnGpu::ScheduleOverlays(
    CandidateList&& overlay_candidates) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

}  // namespace viz
