// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_SERVICE_TEST_BASE_H_
#define SERVICES_DEVICE_DEVICE_SERVICE_TEST_BASE_H_

#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/device_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class DeviceService;

// Base class responsible to setup Device Service for test.
class DeviceServiceTestBase : public testing::Test {
 public:
  DeviceServiceTestBase();

  DeviceServiceTestBase(const DeviceServiceTestBase&) = delete;
  DeviceServiceTestBase& operator=(const DeviceServiceTestBase&) = delete;

  ~DeviceServiceTestBase() override;

  // NOTE: It's important to do service instantiation within SetUp instead of
  // the constructor, as subclasses of this fixture may need to initialize some
  // global state before the service is constructed.
  void SetUp() override;

  void TearDown() override;

 protected:
  mojom::DeviceService* device_service() { return service_remote_.get(); }
  DeviceService* device_service_impl() { return service_.get(); }

  // Can optionally be called to destroy the service before a child test fixture
  // shuts down, in case the DeviceService has dependencies on objects created
  // by the child test fixture.
  void DestroyDeviceService();

  base::test::TaskEnvironment task_environment_;

  // Both of these task runners should be deprecated in favor of individual
  // components of the device service creating their own.
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

 private:
  std::unique_ptr<DeviceService> service_;
  mojo::Remote<mojom::DeviceService> service_remote_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_SERVICE_TEST_BASE_H_
