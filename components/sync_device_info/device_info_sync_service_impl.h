// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync_device_info/device_info_sync_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace syncer {

class DeviceInfoPrefs;
class DeviceInfoSyncClient;
class DeviceInfoSyncBridge;
class MutableLocalDeviceInfoProvider;

class DeviceInfoSyncServiceImpl : public DeviceInfoSyncService {
 public:
  // |local_device_info_provider| must not be null.
  // |device_info_prefs| must not be null.
  // |device_info_sync_client| must not be null and must outlive this object.
  // |pulse_task_runner| must not be null. It will be used to schedule pulses in
  // DeviceInfoSyncBridge.
  DeviceInfoSyncServiceImpl(
      OnceDataTypeStoreFactory data_type_store_factory,
      std::unique_ptr<MutableLocalDeviceInfoProvider>
          local_device_info_provider,
      std::unique_ptr<DeviceInfoPrefs> device_info_prefs,
      std::unique_ptr<DeviceInfoSyncClient> device_info_sync_client,
      scoped_refptr<base::SequencedTaskRunner> pulse_task_runner);

  DeviceInfoSyncServiceImpl(const DeviceInfoSyncServiceImpl&) = delete;
  DeviceInfoSyncServiceImpl& operator=(const DeviceInfoSyncServiceImpl&) =
      delete;

  ~DeviceInfoSyncServiceImpl() override;

  // DeviceInfoSyncService implementation.
  LocalDeviceInfoProvider* GetLocalDeviceInfoProvider() override;
  DeviceInfoTracker* GetDeviceInfoTracker() override;
  base::WeakPtr<DataTypeControllerDelegate> GetControllerDelegate() override;
  void RefreshLocalDeviceInfo() override;

  // KeyedService overrides.
  void Shutdown() override;

 private:
  std::unique_ptr<DeviceInfoSyncClient> device_info_sync_client_;
  std::unique_ptr<DeviceInfoSyncBridge> bridge_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_IMPL_H_
