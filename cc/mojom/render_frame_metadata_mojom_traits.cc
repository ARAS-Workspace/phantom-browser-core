// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojom/render_frame_metadata_mojom_traits.h"

#include <string_view>

#include "base/debug/crash_logging.h"
#include "build/build_config.h"
#include "services/viz/public/cpp/compositing/selection_mojom_traits.h"
#include "services/viz/public/cpp/compositing/tracked_element_rects_mojom_traits.h"
#include "services/viz/public/cpp/compositing/vertical_scroll_direction_mojom_traits.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/selection_bound_mojom_traits.h"

namespace mojo {

namespace {

void SetFailedCheckCrashKey(std::string_view check_name) {
  static auto* const crash_key = base::debug::AllocateCrashKeyString(
      "rfm_failed_check", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(crash_key, check_name);
}

}  // namespace

// static
bool StructTraits<cc::mojom::DelegatedInkBrowserMetadataDataView,
                  cc::DelegatedInkBrowserMetadata>::
    Read(cc::mojom::DelegatedInkBrowserMetadataDataView data,
         cc::DelegatedInkBrowserMetadata* out) {
  out->delegated_ink_is_hovering = data.delegated_ink_is_hovering();
  return true;
}

// static
bool StructTraits<
    cc::mojom::RenderFrameMetadataDataView,
    cc::RenderFrameMetadata>::Read(cc::mojom::RenderFrameMetadataDataView data,
                                   cc::RenderFrameMetadata* out) {
  out->is_scroll_offset_at_top = data.is_scroll_offset_at_top();
  out->is_mobile_optimized = data.is_mobile_optimized();
  out->device_scale_factor = data.device_scale_factor();
  out->page_scale_factor = data.page_scale_factor();
  out->external_page_scale_factor = data.external_page_scale_factor();
  out->top_controls_height = data.top_controls_height();
  out->top_controls_shown_ratio = data.top_controls_shown_ratio();
  out->primary_main_frame_item_sequence_number =
      data.primary_main_frame_item_sequence_number();
  if (!data.ReadRootScrollOffset(&out->root_scroll_offset)) {
    SetFailedCheckCrashKey("root_scroll_offset");
    return false;
  }
  if (!data.ReadSelection(&out->selection)) {
    SetFailedCheckCrashKey("selection");
    return false;
  }
  if (!data.ReadDelegatedInkMetadata(&out->delegated_ink_metadata)) {
    SetFailedCheckCrashKey("delegated_ink_metadata");
    return false;
  }
  if (!data.ReadTrackedElementRects(&out->tracked_element_rects)) {
    SetFailedCheckCrashKey("tracked_element_rects");
    return false;
  }
  if (!data.ReadViewportSizeInPixels(&out->viewport_size_in_pixels)) {
    SetFailedCheckCrashKey("viewport_size_in_pixels");
    return false;
  }
  if (!data.ReadLocalSurfaceId(&out->local_surface_id)) {
    SetFailedCheckCrashKey("local_surface_id");
    return false;
  }
  if (!data.ReadNewVerticalScrollDirection(
          &out->new_vertical_scroll_direction)) {
    SetFailedCheckCrashKey("new_vertical_scroll_direction");
    return false;
  }
  if (!data.ReadRootBackgroundColor(&out->root_background_color)) {
    SetFailedCheckCrashKey("root_background_color");
    return false;
  }
  return true;
}

}  // namespace mojo
