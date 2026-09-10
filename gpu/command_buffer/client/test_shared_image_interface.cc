// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/test_shared_image_interface.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include <utility>


#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

#if BUILDFLAG(IS_LINUX)
#include <linux/memfd.h>
#include <sys/mman.h>
#include <unistd.h>
#endif


namespace gpu {

namespace {

gfx::GpuMemoryBufferType GetNativeBufferType() {
#if BUILDFLAG(IS_APPLE)
  return gfx::IO_SURFACE_BUFFER;
#elif BUILDFLAG(IS_ANDROID)
  return gfx::ANDROID_HARDWARE_BUFFER;
#else
  // Ozone
  return gfx::NATIVE_PIXMAP;
#endif
}

}  // namespace


TestSharedImageInterface::TestSharedImageInterface() {
  InitializeSharedImageCapabilities();
}

TestSharedImageInterface::~TestSharedImageInterface() = default;

// static
gfx::GpuMemoryBufferHandle TestSharedImageInterface::CreateGMBHandle(
    const viz::SharedImageFormat& format,
    const gfx::Size& size) {
  size_t buffer_size =
      viz::SharedMemorySizeForSharedImageFormat(format, size).value();
  CHECK(buffer_size);
  auto shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  CHECK(shared_memory_region.IsValid());

  gfx::GpuMemoryBufferHandle handle(std::move(shared_memory_region));
  handle.offset = 0;
  handle.stride =
      static_cast<uint32_t>(viz::SharedMemoryRowSizeForSharedImageFormat(
                                format, /*plane*/ 0, size.width())
                                .value());

  return handle;
}

scoped_refptr<ClientSharedImage> TestSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    std::optional<SharedImagePoolId> pool_id) {
  SyncToken sync_token = GenUnverifiedSyncToken();
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  most_recent_size_ = si_info.size;
  auto gmb_handle_type = emulate_client_provided_native_buffer_
                             ? GetNativeBufferType()
                             : gfx::EMPTY_BUFFER;
  return base::MakeRefCounted<ClientSharedImage>(mailbox, si_info, sync_token,
                                                 holder_, gmb_handle_type);
}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    base::span<const uint8_t> pixel_data) {
  SyncToken sync_token = GenUnverifiedSyncToken();
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  return base::MakeRefCounted<ClientSharedImage>(mailbox, si_info, sync_token,
                                                 holder_, gfx::EMPTY_BUFFER);
}

scoped_refptr<ClientSharedImage> TestSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    std::optional<SharedImagePoolId> pool_id) {
  DoCreateSharedImage(si_info.size, si_info.format, surface_handle,
                      buffer_usage);
  if (fail_shared_image_creation_with_buffer_usage_) {
    return nullptr;
  }
  SyncToken sync_token = GenUnverifiedSyncToken();

  // Create a ClientSharedImage with a GMB.
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  most_recent_size_ = si_info.size;

  // Copy which can be modified.
  SharedImageInfo si_info_copy = si_info;
  // Set CPU read/write usage based on buffer usage.
  si_info_copy.usage |= GetCpuSIUsage(buffer_usage);

  // Since the GMB handle that we create here is always shared memory, clear
  // the external sampler prefs to avoid a CHECK within ClientSI that external
  // sampling is set only with a native GMB handle.
  if (si_info_copy.format.PrefersExternalSampler()) {
    si_info_copy.format.ClearPrefersExternalSampler();
  }

  auto gmb_handle = CreateGMBHandle(si_info.format, si_info_copy.size);

  auto client_si = base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info_copy, sync_token,
      GpuMemoryBufferHandleInfo(std::move(gmb_handle), buffer_usage), holder_);
  most_recent_mappable_shared_image_ = client_si.get();
  return client_si;
}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  SyncToken sync_token = GenUnverifiedSyncToken();
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  most_recent_size_ = si_info.size;

  // Copy which can be modified.
  SharedImageInfo si_info_copy = si_info;
  // Set CPU read/write usage based on buffer usage.
  si_info_copy.usage |= GetCpuSIUsage(buffer_usage);

  // If the GMB handle passed here is shared memory (e.g. because it was created
  // by a unittest that is simulating a production flow with native handles),
  // clear the external sampler prefs to avoid a CHECK within ClientSI that
  // external sampling is set only with a native GMB handle.
  if (buffer_handle.type == gfx::SHARED_MEMORY_BUFFER &&
      si_info_copy.format.PrefersExternalSampler()) {
    si_info_copy.format.ClearPrefersExternalSampler();
  }

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info_copy, sync_token,
      GpuMemoryBufferHandleInfo(std::move(buffer_handle), buffer_usage),
      holder_);
}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  SyncToken sync_token = GenUnverifiedSyncToken();
  base::AutoLock locked(lock_);
  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  most_recent_size_ = si_info.size;
  return base::MakeRefCounted<ClientSharedImage>(mailbox, si_info, sync_token,
                                                 holder_, buffer_handle.type);
}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImageForSoftwareCompositor(
    const SharedImageInfo& si_info) {
  base::WritableSharedMemoryMapping mapping;
  gfx::GpuMemoryBufferHandle handle;
  CreateSharedMemoryRegionFromSIInfo(si_info, mapping, handle);

  auto mailbox = Mailbox::Generate();
  shared_images_.insert(mailbox);
  most_recent_size_ = si_info.size;

  return base::MakeRefCounted<ClientSharedImage>(
      mailbox, si_info, GenUnverifiedSyncToken(), holder_, std::move(mapping));
}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImageForMLTensor(
    std::string debug_label,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    gpu::SharedImageUsageSet usage) {
  NOTREACHED();
}

void TestSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  base::AutoLock locked(lock_);
  DCHECK(shared_images_.find(mailbox) != shared_images_.end());
  num_update_shared_image_no_fence_calls_++;
}

void TestSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  base::AutoLock locked(lock_);
  DCHECK(shared_images_.find(mailbox) != shared_images_.end());
}

scoped_refptr<ClientSharedImage> TestSharedImageInterface::ImportSharedImage(
    ExportedSharedImage exported_shared_image) {
  shared_images_.insert(exported_shared_image.mailbox_);

  return base::WrapRefCounted<ClientSharedImage>(
      new ClientSharedImage(std::move(exported_shared_image), holder_));
}

void TestSharedImageInterface::DestroySharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  base::AutoLock locked(lock_);
  if (most_recent_mappable_shared_image_ &&
      mailbox == most_recent_mappable_shared_image_->mailbox()) {
    most_recent_mappable_shared_image_ = nullptr;
  }

  shared_images_.erase(mailbox);
  most_recent_destroy_token_ = sync_token;

  if (test_client_) {
    test_client_->DidDestroySharedImage();
  }
}

void TestSharedImageInterface::DestroySharedImage(
    const SyncToken& sync_token,
    scoped_refptr<ClientSharedImage> client_shared_image) {
  CHECK(client_shared_image->HasOneRef());
  client_shared_image->UpdateDestructionSyncToken(sync_token);
}


SyncToken TestSharedImageInterface::GenVerifiedSyncToken() {
  base::AutoLock locked(lock_);
  most_recent_generated_token_ =
      SyncToken(CommandBufferNamespace::GPU_IO,
                CommandBufferId::FromUnsafeValue(static_cast<uint64_t>(
                    GpuChannelReservedRoutes::kSharedImageInterface)),
                ++release_id_);
  VerifySyncToken(most_recent_generated_token_);
  return most_recent_generated_token_;
}

SyncToken TestSharedImageInterface::GenUnverifiedSyncToken() {
  base::AutoLock locked(lock_);
  most_recent_generated_token_ =
      SyncToken(CommandBufferNamespace::GPU_IO,
                CommandBufferId::FromUnsafeValue(static_cast<uint64_t>(
                    GpuChannelReservedRoutes::kSharedImageInterface)),
                ++release_id_);
  return most_recent_generated_token_;
}

void TestSharedImageInterface::VerifySyncToken(SyncToken& sync_token) {
  sync_token.SetVerifyFlush();
}

void TestSharedImageInterface::WaitSyncToken(const SyncToken& sync_token) {
  NOTREACHED();
}

bool TestSharedImageInterface::CanVerifySyncToken(
    const gpu::SyncToken& sync_token) {
  return true;
}

void TestSharedImageInterface::VerifyFlush() {}

scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateSharedImageWithAsyncMapControl(
    const SharedImageInfo& si_info,
    gfx::BufferUsage buffer_usage,
    bool premapped,
    const ClientSharedImage::AsyncMapInvokedCallback& callback) {
  Mailbox mailbox;
  {
    base::AutoLock locked(lock_);
    mailbox = Mailbox::Generate();
    shared_images_.insert(mailbox);
  }

  auto image = ClientSharedImage::CreateForTesting(
      mailbox, si_info, GenUnverifiedSyncToken(), premapped, callback,
      buffer_usage, holder_);
  return image;
}

#if BUILDFLAG(IS_LINUX)
scoped_refptr<ClientSharedImage>
TestSharedImageInterface::CreateNativePixmapBackedSharedImage(
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage) {
  const auto& format = si_info.format;
  const auto& size = si_info.size;

  gfx::NativePixmapHandle native_pixmap_handle;
  for (int i = 0; i < format.NumberOfPlanes(); i++) {
    size_t height_in_pixels = format.GetPlaneSize(i, size).height();
    CHECK(height_in_pixels);
    size_t stride =
        viz::SharedMemoryRowSizeForSharedImageFormat(format, i, size.width())
            .value();

    // Placeholder plane fd.
    const uint64_t plane_size =
        base::CheckMul(height_in_pixels, stride).ValueOrDie<uint64_t>();
    base::ScopedFD fd(memfd_create("test_shared_image", MFD_CLOEXEC));
    CHECK(fd.is_valid());
    CHECK_EQ(ftruncate(fd.get(), plane_size), 0);

    native_pixmap_handle.planes.emplace_back(
        base::checked_cast<uint32_t>(stride), 0, plane_size, std::move(fd));
  }

  return CreateSharedImage(
      si_info, surface_handle, buffer_usage,
      gfx::GpuMemoryBufferHandle(std::move(native_pixmap_handle)));
}
#endif  // BUILDFLAG(IS_LINUX)

bool TestSharedImageInterface::CheckSharedImageExists(
    const Mailbox& mailbox) const {
  base::AutoLock locked(lock_);
  return shared_images_.contains(mailbox);
}

const SharedImageCapabilities&
TestSharedImageInterface::GetCapabilities() {
  return shared_image_capabilities_;
}

void TestSharedImageInterface::SetCapabilities(
    const SharedImageCapabilities& caps) {
  shared_image_capabilities_ = caps;
  InitializeSharedImageCapabilities();
}

void TestSharedImageInterface::InitializeSharedImageCapabilities() {
#if BUILDFLAG(IS_MAC)
  // Initialize `texture_target_for_io_surfaces` to a value that is valid for
  // ClientSharedImage to use, as unittests broadly create and use
  // SharedImageCapabilities instances without initializing this field. The
  // specific value is chosen to match the historical default value that was
  // used when this state was accessed via a global variable.
  if (!shared_image_capabilities_.texture_target_for_io_surfaces) {
    shared_image_capabilities_.texture_target_for_io_surfaces =
        GL_TEXTURE_RECTANGLE_ARB;
  }
#endif
}

}  // namespace gpu
