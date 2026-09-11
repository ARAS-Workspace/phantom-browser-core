// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_sync_service_impl.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_device_info/device_info_sync_bridge.h"
#include "components/sync_device_info/device_info_sync_client.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace syncer {

DeviceInfoSyncServiceImpl::DeviceInfoSyncServiceImpl(
    OnceDataTypeStoreFactory data_type_store_factory,
    std::unique_ptr<MutableLocalDeviceInfoProvider> local_device_info_provider,
    std::unique_ptr<DeviceInfoPrefs> device_info_prefs,
    std::unique_ptr<DeviceInfoSyncClient> device_info_sync_client,
    scoped_refptr<base::SequencedTaskRunner> pulse_task_runner)
    : device_info_sync_client_(std::move(device_info_sync_client)) {
  DCHECK(local_device_info_provider);
  DCHECK(device_info_prefs);
  DCHECK(device_info_sync_client_);

  // Make a copy of the channel to avoid relying on argument evaluation order.
  const version_info::Channel channel =
      local_device_info_provider->GetChannel();

  bridge_ = std::make_unique<DeviceInfoSyncBridge>(
      std::move(local_device_info_provider), std::move(data_type_store_factory),
      std::make_unique<ClientTagBasedDataTypeProcessor>(
          DEVICE_INFO,
          /*dump_stack=*/base::BindRepeating(&ReportUnrecoverableError,
                                             channel)),
      std::move(device_info_prefs), std::move(pulse_task_runner));
}

DeviceInfoSyncServiceImpl::~DeviceInfoSyncServiceImpl() = default;

LocalDeviceInfoProvider*
DeviceInfoSyncServiceImpl::GetLocalDeviceInfoProvider() {
  return bridge_->GetLocalDeviceInfoProvider();
}

DeviceInfoTracker* DeviceInfoSyncServiceImpl::GetDeviceInfoTracker() {
  return bridge_.get();
}

base::WeakPtr<DataTypeControllerDelegate>
DeviceInfoSyncServiceImpl::GetControllerDelegate() {
  return bridge_->change_processor()->GetControllerDelegate();
}

void DeviceInfoSyncServiceImpl::RefreshLocalDeviceInfo() {
  bridge_->RefreshLocalDeviceInfoIfNeeded();
}

void DeviceInfoSyncServiceImpl::Shutdown() {}

}  // namespace syncer
