// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/isolated_xr_device/xr_runtime_provider.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/cpp/features.h"

enum class IsolatedXRRuntimeProvider::RuntimeStatus {
  kEnable,
  kDisable,
};

namespace {
// Poll for device add/remove every 5 seconds.
constexpr base::TimeDelta kTimeBetweenPollingEvents = base::Seconds(5);

template <typename VrDeviceT>
std::unique_ptr<VrDeviceT> CreateDevice() {
  return std::make_unique<VrDeviceT>();
}

template <typename VrDeviceT>
std::unique_ptr<VrDeviceT> EnableRuntime(
    device::mojom::IsolatedXRRuntimeProviderClient* client,
    base::OnceCallback<std::unique_ptr<VrDeviceT>()> create_device) {
  auto device = std::move(create_device).Run();
  TRACE_EVENT_INSTANT("xr", "HardwareAdded", "id",
                      static_cast<int>(device->GetId()));
  // "Device" here refers to a runtime + hardware pair, not necessarily
  // a physical device.
  client->OnDeviceAdded(device->BindXRRuntime(), device->GetDeviceData(),
                        device->GetId());
  return device;
}

template <typename VrDeviceT>
void DisableRuntime(device::mojom::IsolatedXRRuntimeProviderClient* client,
                    std::unique_ptr<VrDeviceT> device) {
  TRACE_EVENT_INSTANT("xr", "HardwareRemoved", "id",
                      static_cast<int>(device->GetId()));
  // "Device" here refers to a runtime + hardware pair, not necessarily physical
  // device.
  client->OnDeviceRemoved(device->GetId());
}

template <typename VrHardwareT>
void SetRuntimeStatus(
    device::mojom::IsolatedXRRuntimeProviderClient* client,
    IsolatedXRRuntimeProvider::RuntimeStatus status,
    base::OnceCallback<std::unique_ptr<VrHardwareT>()> create_device,
    std::unique_ptr<VrHardwareT>* out_device) {
  if (status == IsolatedXRRuntimeProvider::RuntimeStatus::kEnable &&
      !*out_device) {
    *out_device = EnableRuntime<VrHardwareT>(client, std::move(create_device));
  } else if (status == IsolatedXRRuntimeProvider::RuntimeStatus::kDisable &&
             *out_device) {
    DisableRuntime(client, std::move(*out_device));
  }
}

// If none of the runtimes are enabled, this function will be unused.
// This is a bit more scalable than wrapping it in all the typedefs
[[maybe_unused]] bool IsEnabled(const base::CommandLine* command_line,
                                const base::Feature& feature,
                                const std::string& name) {
  if (!command_line->HasSwitch(switches::kWebXrForceRuntime))
    return base::FeatureList::IsEnabled(feature);

  return (base::CompareCaseInsensitiveASCII(
              command_line->GetSwitchValueASCII(switches::kWebXrForceRuntime),
              name) == 0);
}

}  // namespace

// This function is called periodically to check the availability of hardware
// backed by the various supported VR runtimes. Only one "device" (hardware +
// runtime) should be enabled at once, so this chooses the most preferred among
// available options.
void IsolatedXRRuntimeProvider::PollForDeviceChanges() {
  // If none of the following runtimes are enabled, we'll get an error for
  // 'preferred_device_enabled' being unused, thus [[maybe_unused]].
  [[maybe_unused]] bool preferred_device_enabled = false;

  // Schedule this function to run again later.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&IsolatedXRRuntimeProvider::PollForDeviceChanges,
                     weak_ptr_factory_.GetWeakPtr()),
      kTimeBetweenPollingEvents);
}

void IsolatedXRRuntimeProvider::SetupPollingForDeviceChanges() {
  bool any_runtimes_available = false;
  [[maybe_unused]] const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  // If none of the following runtimes are enabled, we'll get an error for
  // 'command_line' being unused, thus [[maybe_unused]].

  // Begin polling for devices
  if (any_runtimes_available) {
    PollForDeviceChanges();
  }
}

void IsolatedXRRuntimeProvider::RequestDevices(
    mojo::PendingRemote<device::mojom::IsolatedXRRuntimeProviderClient>
        client) {
  // Start polling to detect devices being added/removed.
  client_.Bind(std::move(client));
  SetupPollingForDeviceChanges();
  client_->OnDevicesEnumerated();
}

IsolatedXRRuntimeProvider::IsolatedXRRuntimeProvider(
    mojo::PendingRemote<device::mojom::XRDeviceServiceHost> device_service_host,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : device_service_host_(std::move(device_service_host)),
      io_task_runner_(std::move(io_task_runner)) {}

IsolatedXRRuntimeProvider::~IsolatedXRRuntimeProvider() {
}
