// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_shared_memory_util.h"

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#endif


#if BUILDFLAG(IS_APPLE)
#include <mach/vm_map.h>
#endif


namespace base {

static const size_t kDataSize = 1024;

// Common routine used with Posix file descriptors. Check that shared memory
// file descriptor |fd| does not allow writable mappings. Return true on
// success, false otherwise.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
static bool CheckReadOnlySharedMemoryFdPosix(int fd) {
// Note that the error on Android is EPERM, unlike other platforms where
// it will be EACCES.
#if BUILDFLAG(IS_ANDROID)
  const int kExpectedErrno = EPERM;
#else
  const int kExpectedErrno = EACCES;
#endif
  errno = 0;
  void* address =
      mmap(nullptr, kDataSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  const bool success = (address != nullptr) && (address != MAP_FAILED);
  if (success) {
    LOG(ERROR) << "mmap() should have failed!";
    munmap(address, kDataSize);  // Cleanup.
    return false;
  }
  if (errno != kExpectedErrno) {
    PLOG(ERROR) << "Expected mmap() to return " << kExpectedErrno
                << " but returned";  // PLOG will append the actual errno value.
    return false;
  }
  return true;
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE)
bool CheckReadOnlySharedMemoryMachPort(mach_port_t memory_object) {
  vm_address_t memory;
  const kern_return_t kr =
      vm_map(mach_task_self(), &memory, kDataSize, 0, VM_FLAGS_ANYWHERE,
             memory_object, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE,
             VM_PROT_READ | VM_PROT_WRITE | VM_PROT_IS_MASK, VM_INHERIT_NONE);
  if (kr == KERN_SUCCESS) {
    LOG(ERROR) << "vm_map() should have failed!";
    vm_deallocate(mach_task_self(), memory, kDataSize);  // Cleanup.
    return false;
  }
  return true;
}

#endif

bool CheckReadOnlyPlatformSharedMemoryRegionForTesting(
    subtle::PlatformSharedMemoryRegion region) {
  if (region.GetMode() != subtle::PlatformSharedMemoryRegion::Mode::kReadOnly) {
    LOG(ERROR) << "Expected region mode is "
               << static_cast<int>(
                      subtle::PlatformSharedMemoryRegion::Mode::kReadOnly)
               << " but actual is " << static_cast<int>(region.GetMode());
    return false;
  }

#if BUILDFLAG(IS_APPLE)
  return CheckReadOnlySharedMemoryMachPort(region.GetPlatformHandle());
#elif BUILDFLAG(IS_ANDROID)
  return CheckReadOnlySharedMemoryFdPosix(region.GetPlatformHandle());
#else
  return CheckReadOnlySharedMemoryFdPosix(region.GetPlatformHandle().fd);
#endif
}

WritableSharedMemoryMapping MapForTesting(
    subtle::PlatformSharedMemoryRegion* region) {
  return MapAtForTesting(region, 0, region->GetSize());
}

WritableSharedMemoryMapping MapAtForTesting(
    subtle::PlatformSharedMemoryRegion* region,
    uint64_t offset,
    size_t size) {
  SharedMemoryMapper* mapper = SharedMemoryMapper::GetDefaultInstance();
  auto result = region->MapAt(offset, size, mapper);
  if (!result.has_value()) {
    return {};
  }

  return WritableSharedMemoryMapping(result.value(), size, region->GetGUID(),
                                     mapper);
}

template <>
std::pair<ReadOnlySharedMemoryRegion, WritableSharedMemoryMapping>
CreateMappedRegion(size_t size) {
  MappedReadOnlyRegion mapped_region = ReadOnlySharedMemoryRegion::Create(size);
  return {std::move(mapped_region.region), std::move(mapped_region.mapping)};
}

}  // namespace base
