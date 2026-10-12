// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/device_service.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/device_service.h"

namespace content {

namespace {

void BindDeviceServiceReceiver(
    mojo::PendingReceiver<device::mojom::DeviceService> receiver) {
  // Bind the lifetime of the service instance to that of the sequence it's
  // running on.
  static base::SequenceLocalStorageSlot<std::unique_ptr<device::DeviceService>>
      service_slot;
  auto& service = service_slot.GetOrCreateValue();

  if (service) {
    service->AddReceiver(std::move(receiver));
    return;
  }

  auto params = std::make_unique<device::DeviceServiceParams>();

  // This task runner may be used by some device service implementation bits
  // to interface with dbus client code, which in turn imposes some subtle
  // thread affinity on the clients. We therefore require a single-thread
  // runner.
  params->file_task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  params->io_task_runner = GetIOThreadTaskRunner({});

  service = device::CreateDeviceService(std::move(params), std::move(receiver));
}

}  // namespace

device::mojom::DeviceService& GetDeviceService() {
  static base::SequenceLocalStorageSlot<
      mojo::Remote<device::mojom::DeviceService>>
      remote_slot;
  mojo::Remote<device::mojom::DeviceService>& remote =
      remote_slot.GetOrCreateValue();
  if (!remote) {
    // This may be called very early in startup, too early for some Device
    // Service initialization steps (for example, in browser test environments,
    // the Device Service's connection to the Network Service could deadlock).
    // We post a task to defer until the main message loop has started, when
    // initialization is reliably safe.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&BindDeviceServiceReceiver,
                                  remote.BindNewPipeAndPassReceiver()));
  }
  return *remote.get();
}

}  // namespace content
