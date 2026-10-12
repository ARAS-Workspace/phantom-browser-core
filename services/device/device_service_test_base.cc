// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service_test_base.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "services/device/device_service.h"

#if BUILDFLAG(IS_LINUX)
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#endif  // BUILDFLAG(IS_LINUX)

namespace device {

namespace {

std::unique_ptr<DeviceService> CreateTestDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    mojo::PendingReceiver<mojom::DeviceService> receiver) {
  auto params = std::make_unique<DeviceServiceParams>();
  params->file_task_runner = std::move(file_task_runner);
  params->io_task_runner = std::move(io_task_runner);

  return CreateDeviceService(std::move(params), std::move(receiver));
}

}  // namespace

DeviceServiceTestBase::DeviceServiceTestBase()
    : file_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      io_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_VISIBLE})) {}

DeviceServiceTestBase::~DeviceServiceTestBase() = default;

void DeviceServiceTestBase::SetUp() {
  service_ = CreateTestDeviceService(
      file_task_runner_, io_task_runner_,
      service_remote_.BindNewPipeAndPassReceiver());
}

void DeviceServiceTestBase::TearDown() {
#if BUILDFLAG(IS_LINUX)
  task_environment_.RunUntilIdle();
  dbus_thread_linux::ShutdownOnDBusThreadAndBlock();
#endif  // BUILDFLAG(IS_LINUX)
}

void DeviceServiceTestBase::DestroyDeviceService() {
  service_.reset();
}

}  // namespace device
