// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_SWAPCHAIN_INFO_H_
#define DEVICE_VR_OPENXR_OPENXR_SWAPCHAIN_INFO_H_

#include "gpu/command_buffer/client/client_shared_image.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_hardware_buffer_handle.h"
#include "device/vr/android/local_texture.h"  //nogncheck
#include "ui/gl/scoped_egl_image.h"
#endif

namespace device {

// TODO(crbug.com/40909689): Refactor this class.
struct OpenXrSwapchainInfo {
 public:
#if BUILDFLAG(IS_ANDROID)
  explicit OpenXrSwapchainInfo(uint32_t texture);
#endif
  OpenXrSwapchainInfo();
  virtual ~OpenXrSwapchainInfo();
  OpenXrSwapchainInfo(OpenXrSwapchainInfo&&);
  OpenXrSwapchainInfo& operator=(OpenXrSwapchainInfo&&);

  void Clear();

  scoped_refptr<gpu::ClientSharedImage> shared_image;
  gpu::SyncToken sync_token;

#if BUILDFLAG(IS_ANDROID)
  // Ideally this would be a gluint, but there are conflicting headers for GL
  // depending on *how* you want to use it; so we can't use it at the moment.
  uint32_t openxr_texture;

  LocalTexture shared_buffer_texture;

  // The size of the texture used for the shared buffer; which may be different
  // than the size of the actual swapchain image, as this size is influenced by
  // any framebuffer scale factor that the page may request.
  // This property isn't android-specific but it is currently unused on Windows.
  gfx::Size shared_buffer_size{0, 0};

  // This owns a single reference to an AHardwareBuffer object.
  base::android::ScopedHardwareBufferHandle scoped_ahb_handle;

  // This object keeps the image alive while processing a frame. That's
  // required because it owns underlying resources, and must still be
  // alive when the mailbox texture backed by this image is used.
  gl::ScopedEGLImage local_eglimage;
#endif
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SWAPCHAIN_INFO_H_
