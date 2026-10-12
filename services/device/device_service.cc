// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/device/binder_overrides.h"
#include "services/device/fingerprint/fingerprint.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "services/device/generic_sensor/sensor_provider_impl.h"
#include "services/device/geolocation/geolocation_context.h"
#include "services/device/hid/hid_manager_impl.h"
#include "services/device/power_monitor/power_monitor_message_broadcaster.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/time_zone_monitor/time_zone_monitor.h"
#include "services/device/vibration/vibration_manager_impl.h"
#include "services/device/wake_lock/wake_lock_provider.h"

#include "services/device/battery/battery_monitor_impl.h"
#include "services/device/battery/battery_status_service.h"

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
#include "services/device/compute_pressure/pressure_manager_impl.h"
#endif

#if BUILDFLAG(IS_LINUX) && defined(USE_UDEV)
#include "services/device/hid/input_service_linux.h"
#endif

namespace device {

DeviceServiceParams::DeviceServiceParams() = default;

DeviceServiceParams::~DeviceServiceParams() = default;

std::unique_ptr<DeviceService> CreateDeviceService(
    std::unique_ptr<DeviceServiceParams> params,
    mojo::PendingReceiver<mojom::DeviceService> receiver) {
  return std::make_unique<DeviceService>(std::move(params),
                                         std::move(receiver));
}

DeviceService::DeviceService(
    std::unique_ptr<DeviceServiceParams> params,
    mojo::PendingReceiver<mojom::DeviceService> receiver)
    : file_task_runner_(std::move(params->file_task_runner)),
      io_task_runner_(std::move(params->io_task_runner)),
      wake_lock_context_callback_(params->wake_lock_context_callback),
      wake_lock_provider_(file_task_runner_,
                          params->wake_lock_context_callback) {
  receivers_.Add(this, std::move(receiver));

#if !BUILDFLAG(IS_IOS_TVOS)
  // Ensure that the battery backend is initialized now; otherwise it may end up
  // getting initialized on access during destruction, when it's no longer safe
  // to initialize.
  device::BatteryStatusService::GetInstance();
#endif  // !BUILDFLAG(IS_IOS_TVOS)
}

DeviceService::~DeviceService() {
#if !BUILDFLAG(IS_IOS_TVOS)
  device::BatteryStatusService::GetInstance()->Shutdown();
#endif
}

void DeviceService::AddReceiver(
    mojo::PendingReceiver<mojom::DeviceService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DeviceService::SetSensorProviderImplForTesting(
    std::unique_ptr<SensorProviderImpl> sensor_provider) {
  CHECK(!sensor_provider_);
  sensor_provider_ = std::move(sensor_provider);
}

// static
void DeviceService::OverrideGeolocationContextBinderForTesting(
    GeolocationContextBinder binder) {
  internal::GetGeolocationContextBinderOverride() = std::move(binder);
}

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
// static
void DeviceService::OverridePressureManagerBinderForTesting(
    PressureManagerBinder binder) {
  internal::GetPressureManagerBinderOverride() = std::move(binder);
}
#endif

// static
void DeviceService::OverrideTimeZoneMonitorBinderForTesting(
    TimeZoneMonitorBinder binder) {
  internal::GetTimeZoneMonitorBinderOverride() = std::move(binder);
}

void DeviceService::BindBatteryMonitor(
    mojo::PendingReceiver<mojom::BatteryMonitor> receiver) {
#if !BUILDFLAG(IS_IOS_TVOS)
  BatteryMonitorImpl::Create(std::move(receiver));
#endif
}

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
void DeviceService::BindPressureManager(
    mojo::PendingReceiver<mojom::PressureManager> receiver) {
  const auto& binder_override = internal::GetPressureManagerBinderOverride();
  if (binder_override) {
    binder_override.Run(std::move(receiver));
    return;
  }

  if (!pressure_manager_)
    pressure_manager_ = PressureManagerImpl::Create();
  pressure_manager_->Bind(std::move(receiver));
}
#endif

void DeviceService::BindVibrationManager(
    mojo::PendingReceiver<mojom::VibrationManager> receiver,
    mojo::PendingRemote<mojom::VibrationManagerListener> listener) {
  VibrationManagerImpl::Create(std::move(receiver), std::move(listener));
}

void DeviceService::BindHidManager(
    mojo::PendingReceiver<mojom::HidManager> receiver) {
  if (!hid_manager_) {
    hid_manager_ = std::make_unique<HidManagerImpl>();
  }
  hid_manager_->AddReceiver(std::move(receiver));
}

#if BUILDFLAG(IS_LINUX) && defined(USE_UDEV)
void DeviceService::BindInputDeviceManager(
    mojo::PendingReceiver<mojom::InputDeviceManager> receiver) {
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputServiceLinux::BindReceiver, std::move(receiver)));
}
#endif

void DeviceService::BindFingerprint(
    mojo::PendingReceiver<mojom::Fingerprint> receiver) {
  Fingerprint::Create(std::move(receiver));
}

void DeviceService::BindGeolocationContext(
    mojo::PendingReceiver<mojom::GeolocationContext> receiver) {
  const auto& binder_override = internal::GetGeolocationContextBinderOverride();
  if (binder_override) {
    binder_override.Run(std::move(receiver));
    return;
  }

  GeolocationContext::Create(std::move(receiver));
}

void DeviceService::BindPowerMonitor(
    mojo::PendingReceiver<mojom::PowerMonitor> receiver) {
  if (!power_monitor_message_broadcaster_) {
    power_monitor_message_broadcaster_ =
        std::make_unique<PowerMonitorMessageBroadcaster>();
  }
  power_monitor_message_broadcaster_->Bind(std::move(receiver));
}

void DeviceService::BindScreenOrientationListener(
    mojo::PendingReceiver<mojom::ScreenOrientationListener> receiver) {
}

void DeviceService::BindSensorProvider(
    mojo::PendingReceiver<mojom::SensorProvider> receiver) {
  if (!sensor_provider_) {
    auto platform_provider = PlatformSensorProvider::Create();
    if (!platform_provider)
      return;
    sensor_provider_ =
        std::make_unique<SensorProviderImpl>(std::move(platform_provider));
  }
  sensor_provider_->Bind(std::move(receiver));
}

void DeviceService::BindTimeZoneMonitor(
    mojo::PendingReceiver<mojom::TimeZoneMonitor> receiver) {
  const auto& binder_override = internal::GetTimeZoneMonitorBinderOverride();
  if (binder_override) {
    binder_override.Run(std::move(receiver));
    return;
  }

  if (!time_zone_monitor_)
    time_zone_monitor_ = TimeZoneMonitor::Create(file_task_runner_);
  time_zone_monitor_->Bind(std::move(receiver));
}

void DeviceService::BindWakeLockProvider(
    mojo::PendingReceiver<mojom::WakeLockProvider> receiver) {
  wake_lock_provider_.AddBinding(std::move(receiver));
}

}  // namespace device
