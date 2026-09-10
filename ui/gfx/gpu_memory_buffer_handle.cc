// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer_handle.h"

#include "base/logging.h"
#include "base/notimplemented.h"
#include "build/build_config.h"
#include "ui/gfx/generic_shared_memory_id.h"

namespace gfx {

GpuMemoryBufferHandle::GpuMemoryBufferHandle() = default;

GpuMemoryBufferHandle::GpuMemoryBufferHandle(
    base::UnsafeSharedMemoryRegion region)
    : type(GpuMemoryBufferType::SHARED_MEMORY_BUFFER),
      region_(std::move(region)) {
  CHECK(region_.IsValid(), base::NotFatalUntil::M155);
}

#if BUILDFLAG(IS_OZONE)
GpuMemoryBufferHandle::GpuMemoryBufferHandle(
    NativePixmapHandle native_pixmap_handle)
    : type(GpuMemoryBufferType::NATIVE_PIXMAP),
      native_pixmap_handle_(std::move(native_pixmap_handle)) {}
#endif  // BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_ANDROID)
GpuMemoryBufferHandle::GpuMemoryBufferHandle(
    base::android::ScopedHardwareBufferHandle handle)
    : type(GpuMemoryBufferType::ANDROID_HARDWARE_BUFFER),
      android_hardware_buffer(std::move(handle)) {}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_APPLE)
GpuMemoryBufferHandle::GpuMemoryBufferHandle(ScopedIOSurface io_surface)
    : type(GpuMemoryBufferType::IO_SURFACE_BUFFER),
      io_surface_(std::move(io_surface)) {
  CHECK(io_surface_);
#if BUILDFLAG(IS_IOS)
  io_surface_mach_port_.reset(IOSurfaceCreateMachPort(io_surface_.get()));
  ExportIOSurfaceSharedMemoryRegion(
      io_surface_.get(), io_surface_shared_memory_region_,
      io_surface_plane_strides_, io_surface_plane_offsets_);
#endif  // BUILDFLAG(IS_IOS)
}
#endif  // BUILDFLAG(IS_APPLE)

// TODO(crbug.com/40584691): Reset |type| and possibly the handles on the
// moved-from object.
GpuMemoryBufferHandle::GpuMemoryBufferHandle(GpuMemoryBufferHandle&& other) =
    default;

GpuMemoryBufferHandle& GpuMemoryBufferHandle::operator=(
    GpuMemoryBufferHandle&& other) = default;

GpuMemoryBufferHandle::~GpuMemoryBufferHandle() = default;

GpuMemoryBufferHandle GpuMemoryBufferHandle::Clone() const {
  GpuMemoryBufferHandle handle;
  handle.type = type;
  handle.offset = offset;
  handle.stride = stride;
#if BUILDFLAG(IS_OZONE)
  handle.native_pixmap_handle_ = CloneHandleForIPC(native_pixmap_handle_);
#elif BUILDFLAG(IS_APPLE)
  handle.io_surface_ = io_surface_;
#if BUILDFLAG(IS_IOS)
  handle.io_surface_mach_port_ = io_surface_mach_port_;
  handle.io_surface_shared_memory_region_ =
      io_surface_shared_memory_region_.Duplicate();
  handle.io_surface_plane_strides_ = io_surface_plane_strides_;
  handle.io_surface_plane_offsets_ = io_surface_plane_offsets_;
#endif
#elif BUILDFLAG(IS_ANDROID)
  if (android_hardware_buffer.is_valid()) {
    handle.android_hardware_buffer = android_hardware_buffer.Clone();
  }
#endif
  handle.region_ = region_.Duplicate();
  return handle;
}

}  // namespace gfx
