// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_
#define CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

class IsolatedXRRuntimeProvider final
    : public device::mojom::IsolatedXRRuntimeProvider {
 public:
  explicit IsolatedXRRuntimeProvider(
      mojo::PendingRemote<device::mojom::XRDeviceServiceHost>
          device_service_host,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~IsolatedXRRuntimeProvider() override;

  void RequestDevices(
      mojo::PendingRemote<device::mojom::IsolatedXRRuntimeProviderClient>
          client) override;

  enum class RuntimeStatus;

 private:
  void PollForDeviceChanges();
  void SetupPollingForDeviceChanges();

  mojo::Remote<device::mojom::XRDeviceServiceHost> device_service_host_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  mojo::Remote<device::mojom::IsolatedXRRuntimeProviderClient> client_;
  base::WeakPtrFactory<IsolatedXRRuntimeProvider> weak_ptr_factory_{this};
};

#endif  // CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_
