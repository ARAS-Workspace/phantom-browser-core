// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_SERVICE_H_
#define SERVICES_DEVICE_DEVICE_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/cpp/compute_pressure/buildflags.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/device_service.mojom.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/power_monitor.mojom.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "services/device/public/mojom/time_zone_monitor.mojom.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/device/wake_lock/wake_lock_context.h"
#include "services/device/wake_lock/wake_lock_provider.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#include "services/device/public/mojom/hid.mojom.h"

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
#include "services/device/public/mojom/pressure_manager.mojom.h"
#endif

#if BUILDFLAG(IS_LINUX) && defined(USE_UDEV)
#include "services/device/public/mojom/input_service.mojom.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
class PressureManagerImpl;
#endif

class DeviceService;
class HidManagerImpl;
class PowerMonitorMessageBroadcaster;
class SensorProviderImpl;
class TimeZoneMonitor;

// NOTE: See the comments on the definition of |WakeLockContextCallback| and
// NFCDelegate.java to understand the semantics and usage of these parameters.
struct DeviceServiceParams {
  DeviceServiceParams();
  ~DeviceServiceParams();

  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner;
  WakeLockContextCallback wake_lock_context_callback;
};

std::unique_ptr<DeviceService> CreateDeviceService(
    std::unique_ptr<DeviceServiceParams> params,
    mojo::PendingReceiver<mojom::DeviceService> receiver);

class DeviceService : public mojom::DeviceService {
 public:
  DeviceService(std::unique_ptr<DeviceServiceParams> params,
                mojo::PendingReceiver<mojom::DeviceService> receiver);

  DeviceService(const DeviceService&) = delete;
  DeviceService& operator=(const DeviceService&) = delete;

  ~DeviceService() override;

  void AddReceiver(mojo::PendingReceiver<mojom::DeviceService> receiver);

  void SetSensorProviderImplForTesting(
      std::unique_ptr<SensorProviderImpl> sensor_provider);

  // Supports global override of GeolocationContext binding within the service.
  using GeolocationContextBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::GeolocationContext>)>;
  static void OverrideGeolocationContextBinderForTesting(
      GeolocationContextBinder binder);

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  // Supports global override of PressureManager binding within the service.
  using PressureManagerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::PressureManager>)>;
  static void OverridePressureManagerBinderForTesting(
      PressureManagerBinder binder);
#endif

  // Supports global override of TimeZoneMonitor binding within the service.
  using TimeZoneMonitorBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::TimeZoneMonitor>)>;
  static void OverrideTimeZoneMonitorBinderForTesting(
      TimeZoneMonitorBinder binder);

 private:
  // mojom::DeviceService implementation:
  void BindFingerprint(
      mojo::PendingReceiver<mojom::Fingerprint> receiver) override;
  void BindGeolocationContext(
      mojo::PendingReceiver<mojom::GeolocationContext> receiver) override;

#if BUILDFLAG(IS_LINUX) && defined(USE_UDEV)
  void BindInputDeviceManager(
      mojo::PendingReceiver<mojom::InputDeviceManager> receiver) override;
#endif

  void BindBatteryMonitor(
      mojo::PendingReceiver<mojom::BatteryMonitor> receiver) override;

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  void BindPressureManager(
      mojo::PendingReceiver<mojom::PressureManager> receiver) override;
#endif

  void BindVibrationManager(
      mojo::PendingReceiver<mojom::VibrationManager> receiver,
      mojo::PendingRemote<mojom::VibrationManagerListener> listener) override;

  void BindHidManager(
      mojo::PendingReceiver<mojom::HidManager> receiver) override;

  void BindPowerMonitor(
      mojo::PendingReceiver<mojom::PowerMonitor> receiver) override;

  void BindScreenOrientationListener(
      mojo::PendingReceiver<mojom::ScreenOrientationListener> receiver)
      override;

  void BindSensorProvider(
      mojo::PendingReceiver<mojom::SensorProvider> receiver) override;

  void BindTimeZoneMonitor(
      mojo::PendingReceiver<mojom::TimeZoneMonitor> receiver) override;

  void BindWakeLockProvider(
      mojo::PendingReceiver<mojom::WakeLockProvider> receiver) override;

  mojo::ReceiverSet<mojom::DeviceService> receivers_;
  std::unique_ptr<PowerMonitorMessageBroadcaster>
      power_monitor_message_broadcaster_;
  std::unique_ptr<SensorProviderImpl> sensor_provider_;
  std::unique_ptr<TimeZoneMonitor> time_zone_monitor_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  WakeLockContextCallback wake_lock_context_callback_;
  WakeLockProvider wake_lock_provider_;
  std::unique_ptr<HidManagerImpl> hid_manager_;

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  std::unique_ptr<PressureManagerImpl> pressure_manager_;
#endif

};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_SERVICE_H_
