// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_
#define CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/services/printing/print_backend_service_impl.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/backend/test_print_backend.h"

namespace printing {

// `PrintBackendServiceTestImpl` uses a `TestPrintBackend` to enable testing
// of the `PrintBackendService` without relying upon the presence of real
// printer drivers.
class PrintBackendServiceTestImpl : public PrintBackendServiceImpl {
 public:
  // Launch the service in-process for testing using the provided backend.
  // `sandboxed` identifies if this service is potentially subject to
  // experiencing access-denied errors on some commands.
  static std::unique_ptr<PrintBackendServiceTestImpl> LaunchForTesting(
      mojo::Remote<mojom::PrintBackendService>& remote,
      scoped_refptr<TestPrintBackend> backend,
      bool sandboxed);

  PrintBackendServiceTestImpl(const PrintBackendServiceTestImpl&) = delete;
  PrintBackendServiceTestImpl& operator=(const PrintBackendServiceTestImpl&) =
      delete;
  ~PrintBackendServiceTestImpl() override;

  // Override which needs special handling for using `test_print_backend_`.
  void Init(const std::string& locale) override;

  // Overrides to support testing service termination scenarios.
  void EnumeratePrinters(
      mojom::PrintBackendService::EnumeratePrintersCallback callback) override;
  void GetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback)
      override;
#if BUILDFLAG(IS_CHROMEOS)
  void GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
          callback) override;
#endif
  void FetchCapabilities(
      const std::string& printer_name,
      mojom::PrintBackendService::FetchCapabilitiesCallback callback) override;
  void UpdatePrintSettings(
      uint32_t context_id,
      base::DictValue job_settings,
      mojom::PrintBackendService::UpdatePrintSettingsCallback callback)
      override;

  // Tests which will have a leftover printing context established in the
  // service can use this to skip the destructor check that all contexts were
  // cleaned up.
  void SkipPersistentContextsCheckOnShutdown() {
    skip_dtor_persistent_contexts_check_ = true;
  }

  // Cause the service to terminate on the next interaction it receives.  Once
  // terminated no further Mojo calls will be possible since there will not be
  // a receiver to handle them.
  void SetTerminateReceiverOnNextInteraction() { terminate_receiver_ = true; }

 private:
  // Use LaunchForTesting() or LaunchForTestingWithServiceThread().
  PrintBackendServiceTestImpl(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver,
      bool is_sandboxed,
      scoped_refptr<TestPrintBackend> backend);

  void OnDidGetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback,
      mojom::PrintBackendService::GetDefaultPrinterNameResult printer_name);

  void TerminateConnection();

  // When pretending to be sandboxed, have the possibility of getting access
  // denied errors.
  const bool is_sandboxed_;

  // Marker for skipping check for empty persistent contexts at destruction.
  bool skip_dtor_persistent_contexts_check_ = false;

  // Marker to signal service should terminate on next interaction.
  bool terminate_receiver_ = false;

  scoped_refptr<TestPrintBackend> test_print_backend_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_
