// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_connection.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_io_thread.h"
#include "build/build_config.h"
#include "services/device/hid/hid_connection.h"
#include "services/device/hid/hid_service.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/hid/hid_blocklist.h"
#include "services/device/public/cpp/hid/hid_collection.h"
#include "services/device/public/cpp/test/test_report_descriptors.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::base::test::TestFuture;

}  // namespace

namespace {

// A test implementation of HidConnection where the platform operations always
// succeed.
class TestHidConnection : public HidConnection {
 public:
  TestHidConnection(scoped_refptr<HidDeviceInfo> device_info,
                    bool allow_protected_reports,
                    bool allow_fido_reports)
      : HidConnection(device_info,
                      allow_protected_reports,
                      allow_fido_reports) {}
  TestHidConnection(const TestHidConnection&) = delete;
  TestHidConnection& operator=(TestHidConnection&) = delete;

  void PlatformClose() override {}
  void PlatformWrite(scoped_refptr<base::RefCountedBytes> buffer,
                     WriteCallback callback) override {
    std::move(callback).Run(/*success=*/true);
  }
  void PlatformGetFeatureReport(uint8_t report_id,
                                ReadCallback callback) override {
    auto buffer =
        base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{42});
    std::move(callback).Run(/*success=*/true, buffer, buffer->size());
  }
  void PlatformSendFeatureReport(scoped_refptr<base::RefCountedBytes> buffer,
                                 WriteCallback callback) override {
    std::move(callback).Run(/*success=*/true);
  }
  void SimulateInputReport(scoped_refptr<base::RefCountedBytes> buffer) {
    ProcessInputReport(buffer, buffer->size());
  }

 private:
  ~TestHidConnection() override = default;
};

}  // namespace

class HidConnectionProtectedReportTest : public testing::Test,
                                         HidConnection::Client {
 public:
  HidConnectionProtectedReportTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kWebHidRecursiveFiltering);
  }
  HidConnectionProtectedReportTest(const HidConnectionProtectedReportTest&) =
      delete;
  HidConnectionProtectedReportTest& operator=(
      const HidConnectionProtectedReportTest&) = delete;
  ~HidConnectionProtectedReportTest() override = default;

  scoped_refptr<HidDeviceInfo> CreateHidDeviceInfo(
      base::span<const uint8_t> report_descriptor,
      uint16_t vendor_id = 0x1234,
      uint16_t product_id = 0xabcd) {
#if BUILDFLAG(IS_MAC)
    const uint64_t kTestDeviceId = 0;
#else
    const char* const kTestDeviceId = "0";
#endif
    return base::MakeRefCounted<HidDeviceInfo>(
        kTestDeviceId, "physical-device-id", vendor_id, product_id,
        "product-name", "serial-number", mojom::HidBusType::kHIDBusTypeUSB,
        report_descriptor);
  }

  scoped_refptr<HidDeviceInfo> CreateHidDeviceInfo(
      mojom::HidCollectionInfoPtr collection,
      size_t max_input_report_size,
      size_t max_output_report_size,
      size_t max_feature_report_size,
      uint16_t vendor_id = 0x1234,
      uint16_t product_id = 0xabcd) {
#if BUILDFLAG(IS_MAC)
    const uint64_t kTestDeviceId = 0;
#else
    const char* const kTestDeviceId = "0";
#endif
    return base::MakeRefCounted<HidDeviceInfo>(
        kTestDeviceId, "physical-device-id", "interface-id", vendor_id,
        product_id, "product-name", "serial-number",
        mojom::HidBusType::kHIDBusTypeUSB, std::move(collection),
        max_input_report_size, max_output_report_size, max_feature_report_size);
  }

  void CreateConnection(scoped_refptr<HidDeviceInfo> device_info,
                        bool allow_protected_reports = false,
                        bool allow_fido_reports = false) {
    connection_ = base::MakeRefCounted<TestHidConnection>(
        device_info, allow_protected_reports, allow_fido_reports);
  }

  void SetConnectionClient() { connection_->SetClient(this); }

  TestHidConnection& connection() { return *connection_.get(); }

  bool HasNextInputReport() { return input_report_future_.IsReady(); }

  std::pair<scoped_refptr<base::RefCountedBytes>, size_t>
  TakeNextInputReport() {
    return input_report_future_.Take();
  }

 private:
  // HidConnection::Client:
  void OnInputReport(scoped_refptr<base::RefCountedBytes> buffer,
                     size_t size) override {
    input_report_future_.SetValue(buffer, size);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<TestHidConnection> connection_;
  base::test::TestFuture<scoped_refptr<base::RefCountedBytes>, size_t>
      input_report_future_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HidConnectionProtectedReportTest, UnprotectedReadWrite) {
  // Simulate a connection to a device with input, output, and feature reports.
  // No reports are protected.
  auto device_info =
      CreateHidDeviceInfo(TestReportDescriptors::SonyDualshock3Usb());
  ASSERT_TRUE(device_info);
  CreateConnection(device_info);

  // Simulate an input report and read it.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t> read_future;
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{1});
  connection().SimulateInputReport(buffer);
  connection().Read(read_future.GetCallback());
  EXPECT_TRUE(read_future.Get<0>());

  SetConnectionClient();

  // Simulate another input report. This time it should notify the connection
  // client.
  connection().SimulateInputReport(buffer);
  TakeNextInputReport();

  // Write an output report.
  TestFuture<bool> write_future;
  connection().Write(buffer, write_future.GetCallback());
  EXPECT_TRUE(write_future.Get());

  // Get a feature report.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t>
      get_feature_future;
  connection().GetFeatureReport(/*report_id=*/1,
                                get_feature_future.GetCallback());
  EXPECT_TRUE(get_feature_future.Get<0>());

  // Send a feature report.
  TestFuture<bool> send_feature_future;
  connection().SendFeatureReport(buffer, send_feature_future.GetCallback());
  EXPECT_TRUE(send_feature_future.Get());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest, ProtectedReportsWithClient) {
  // Simulate a connection to a device with protected input and output reports
  // and an unprotected feature report. Input and output reports are protected
  // because the device has the Generic Desktop Keyboard usage.
  auto device_info =
      CreateHidDeviceInfo(TestReportDescriptors::RfideasPcproxBadgeReader());
  ASSERT_TRUE(device_info);
  CreateConnection(device_info);

  SetConnectionClient();

  // Simulate an input report after setting the connection client. The
  // report should not be received by the client.
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{0});
  connection().SimulateInputReport(buffer);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasNextInputReport());

  // Try to write a protected output report. It should be blocked.
  TestFuture<bool> write_future;
  connection().Write(buffer, write_future.GetCallback());
  EXPECT_FALSE(write_future.Get());

  // Get a feature report. It should succeed.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t>
      get_feature_future;
  connection().GetFeatureReport(/*report_id=*/0,
                                get_feature_future.GetCallback());
  EXPECT_TRUE(get_feature_future.Get<0>());

  // Send a feature report. It should succeed.
  TestFuture<bool> send_feature_future;
  connection().SendFeatureReport(buffer, send_feature_future.GetCallback());
  EXPECT_TRUE(send_feature_future.Get());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest, ProtectedInputReportWithoutClient) {
  // Simulate a connection to a device with protected input reports.
  auto device_info = CreateHidDeviceInfo(TestReportDescriptors::Mouse());
  ASSERT_TRUE(device_info);
  CreateConnection(device_info);

  // Simulate an input report. It should be ignored.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t> read_future;
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{0});
  connection().SimulateInputReport(buffer);
  connection().Read(read_future.GetCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(read_future.IsReady());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest, AllowProtectedReportsAllowsFido) {
  // Simulate a blocklisted FIDO device.
  auto device_info = CreateHidDeviceInfo(TestReportDescriptors::FidoU2fHid());
  ASSERT_TRUE(device_info);
  ASSERT_EQ(device_info->collections().size(), 1u);
  ASSERT_EQ(device_info->collections()[0]->usage->usage_page, mojom::kPageFido);

  // Simulate a connection from a client allowed to access protected reports.
  CreateConnection(device_info, /*allow_protected_reports=*/true);

  // Simulate an input report.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t> read_future;
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{0});
  connection().SimulateInputReport(buffer);
  connection().Read(read_future.GetCallback());
  EXPECT_TRUE(read_future.Get<0>());

  // Simulate an output report.
  TestFuture<bool> write_future;
  connection().Write(buffer, write_future.GetCallback());
  EXPECT_TRUE(write_future.Get());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest, AllowFidoReportsAllowsFido) {
  // Simulate a FIDO security key.
  auto device_info = CreateHidDeviceInfo(TestReportDescriptors::FidoU2fHid());
  ASSERT_TRUE(device_info);
  ASSERT_EQ(device_info->collections().size(), 1u);
  ASSERT_EQ(device_info->collections()[0]->usage->usage_page, mojom::kPageFido);

  // Simulate a connection from a FIDO-privileged origin.
  CreateConnection(device_info, /*allow_protected_reports=*/false,
                   /*allow_fido_reports=*/true);

  // Simulate an input report.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t> read_future;
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{0});
  connection().SimulateInputReport(buffer);
  connection().Read(read_future.GetCallback());
  EXPECT_TRUE(read_future.Get<0>());

  // Simulate an output report.
  TestFuture<bool> write_future;
  connection().Write(buffer, write_future.GetCallback());
  EXPECT_TRUE(write_future.Get());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest,
       AllowFidoReportsAllowsNonFidoSecurityKey) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSecurityKeyHidInterfacesAreFido);

  // Simulate a known security key without FIDO capabilities.
  auto known_security_keys =
      device::HidBlocklist::GetKnownSecurityKeysForTesting();
  ASSERT_GE(known_security_keys.size(), 1u);
  const auto& [vendor_id, product_id] = known_security_keys.front();
  auto device_info = CreateHidDeviceInfo(
      TestReportDescriptors::VendorDefinedInputOutput(), vendor_id, product_id);
  ASSERT_TRUE(device_info);
  ASSERT_TRUE(HidBlocklist::IsKnownSecurityKey(device_info->vendor_id(),
                                               device_info->product_id()));

  // Simulate a connection from a FIDO-privileged origin.
  CreateConnection(device_info, /*allow_protected_reports=*/false,
                   /*allow_fido_reports=*/true);

  // Simulate an input report.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t> read_future;
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{0});
  connection().SimulateInputReport(buffer);
  connection().Read(read_future.GetCallback());
  EXPECT_TRUE(read_future.Get<0>());

  // Simulate an output report.
  TestFuture<bool> write_future;
  connection().Write(buffer, write_future.GetCallback());
  EXPECT_TRUE(write_future.Get());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest, InvisibleConstFeatureReport) {
  auto collection = HidCollection(nullptr, mojom::kPageGenericDesktop,
                                  mojom::kGenericDesktopMouse,
                                  mojom::kHIDCollectionTypeApplication);
  auto device_info_size = CreateHidDeviceInfo(collection.ToMojo(), 0, 0,
                                              /*max_feature_report_size=*/1);
  CreateConnection(device_info_size);

  // Read protected invisible input report.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t> read_future;
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{0});
  connection().SimulateInputReport(buffer);
  connection().Read(read_future.GetCallback());
  EXPECT_FALSE(read_future.Get<0>());

  SetConnectionClient();

  // Write protected invisible output report.
  TestFuture<bool> write_future;
  connection().Write(buffer, write_future.GetCallback());
  EXPECT_FALSE(write_future.Get());

  // Read unprotected invisible feature report.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t>
      get_feature_future;
  connection().GetFeatureReport(/*report_id=*/0,
                                get_feature_future.GetCallback());
  EXPECT_TRUE(get_feature_future.Get<0>());

  // Write unprotected invisible feature report.
  TestFuture<bool> send_feature_future;
  connection().SendFeatureReport(buffer, send_feature_future.GetCallback());
  EXPECT_TRUE(send_feature_future.Get());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest, FidoReportsInNestedCollectionBlocked) {
  // Simulate a device with a vendor-defined top-level collection containing a
  // nested FIDO application collection that defines input and output reports.
  auto device_info =
      CreateHidDeviceInfo(TestReportDescriptors::VendorWithNestedFido());
  ASSERT_TRUE(device_info);
  ASSERT_EQ(device_info->collections().size(), 1u);
  EXPECT_EQ(device_info->collections()[0]->usage->usage_page,
            mojom::kPageVendor);
  CreateConnection(device_info);

  SetConnectionClient();

  // Simulate an input report from the nested FIDO collection. It should not be
  // received by the client.
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{1});
  connection().SimulateInputReport(buffer);
  EXPECT_FALSE(HasNextInputReport());

  // Try to write an output report to the nested FIDO collection. It should be
  // blocked.
  TestFuture<bool> write_future;
  connection().Write(buffer, write_future.GetCallback());
  EXPECT_FALSE(write_future.Get());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest,
       AllowFidoReportsAllowsFidoInNestedCollection) {
  // Simulate a device with a vendor-defined top-level collection containing a
  // nested FIDO application collection that defines input and output reports.
  auto device_info =
      CreateHidDeviceInfo(TestReportDescriptors::VendorWithNestedFido());
  ASSERT_TRUE(device_info);

  // Simulate a connection from a FIDO-privileged origin.
  CreateConnection(device_info, /*allow_protected_reports=*/false,
                   /*allow_fido_reports=*/true);

  // Simulate an input report.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t> read_future;
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{1});
  connection().SimulateInputReport(buffer);
  connection().Read(read_future.GetCallback());
  EXPECT_TRUE(read_future.Get<0>());

  // Simulate an output report.
  TestFuture<bool> write_future;
  connection().Write(buffer, write_future.GetCallback());
  EXPECT_TRUE(write_future.Get());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest,
       KeyboardReportsInNestedCollectionBlocked) {
  // Simulate a device with a vendor-defined top-level collection containing a
  // nested keyboard application collection that defines an input report.
  auto device_info =
      CreateHidDeviceInfo(TestReportDescriptors::VendorWithNestedKeyboard());
  ASSERT_TRUE(device_info);
  ASSERT_EQ(device_info->collections().size(), 1u);
  EXPECT_EQ(device_info->collections()[0]->usage->usage_page,
            mojom::kPageVendor);
  CreateConnection(device_info);

  SetConnectionClient();

  // Simulate an input report from the nested keyboard collection. It should
  // not be received by the client.
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{1});
  connection().SimulateInputReport(buffer);
  EXPECT_FALSE(HasNextInputReport());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest, CollectionTypeAffectsProtection) {
  constexpr uint8_t kTestReportId = 1;

  // Case 1: Collection type is Physical (0). Reports should NOT be protected.
  {
    auto collection = mojom::HidCollectionInfo::New();
    collection->usage = mojom::HidUsageAndPage::New(
        mojom::kGenericDesktopKeyboard, mojom::kPageGenericDesktop);
    collection->collection_type = mojom::kHIDCollectionTypePhysical;
    collection->report_ids.push_back(kTestReportId);
    auto report = mojom::HidReportDescription::New();
    report->report_id = kTestReportId;
    collection->input_reports.push_back(std::move(report));

    auto device_info = CreateHidDeviceInfo(std::move(collection),
                                           /*max_input_report_size=*/1,
                                           /*max_output_report_size=*/0,
                                           /*max_feature_report_size=*/0);
    CreateConnection(device_info);

    TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t> read_future;
    auto buffer = base::MakeRefCounted<base::RefCountedBytes>(
        std::vector<uint8_t>{kTestReportId});
    connection().SimulateInputReport(buffer);
    connection().Read(read_future.GetCallback());
    EXPECT_TRUE(read_future.Get<0>());
    connection().Close();
  }

  // Case 2: Collection type is Application (1). Reports SHOULD be protected.
  {
    auto collection = mojom::HidCollectionInfo::New();
    collection->usage = mojom::HidUsageAndPage::New(
        mojom::kGenericDesktopKeyboard, mojom::kPageGenericDesktop);
    collection->collection_type = mojom::kHIDCollectionTypeApplication;
    collection->report_ids.push_back(kTestReportId);
    auto report = mojom::HidReportDescription::New();
    report->report_id = kTestReportId;
    collection->input_reports.push_back(std::move(report));

    auto device_info = CreateHidDeviceInfo(std::move(collection),
                                           /*max_input_report_size=*/1,
                                           /*max_output_report_size=*/0,
                                           /*max_feature_report_size=*/0);
    CreateConnection(device_info);

    TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t> read_future;
    auto buffer = base::MakeRefCounted<base::RefCountedBytes>(
        std::vector<uint8_t>{kTestReportId});
    connection().SimulateInputReport(buffer);
    connection().Read(read_future.GetCallback());
    EXPECT_FALSE(read_future.IsReady());
    connection().Close();
  }
}

}  // namespace device
