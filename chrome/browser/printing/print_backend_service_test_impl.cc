// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_backend_service_test_impl.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "printing/backend/test_print_backend.h"

namespace printing {

PrintBackendServiceTestImpl::PrintBackendServiceTestImpl(
    mojo::PendingReceiver<mojom::PrintBackendService> receiver,
    bool is_sandboxed,
    scoped_refptr<TestPrintBackend> backend)
    : PrintBackendServiceImpl(std::move(receiver)),
      is_sandboxed_(is_sandboxed),
      test_print_backend_(std::move(backend)) {}

PrintBackendServiceTestImpl::~PrintBackendServiceTestImpl() {
  if (!skip_dtor_persistent_contexts_check_) {
    // Make sure that all persistent contexts have been properly cleaned up.
    DCHECK(persistent_printing_contexts_.empty());
  }
  if (is_sandboxed_) {
    PrintBackendServiceManager::GetInstance().SetServiceForTesting(nullptr);
  } else {
    PrintBackendServiceManager::GetInstance().SetServiceForFallbackTesting(
        nullptr);
  }
}

void PrintBackendServiceTestImpl::Init(const std::string& locale) {
  DCHECK(test_print_backend_);
  print_backend_ = test_print_backend_;
  InitCommon(locale);
}

void PrintBackendServiceTestImpl::EnumeratePrinters(
    mojom::PrintBackendService::EnumeratePrintersCallback callback) {
  if (terminate_receiver_) {
    TerminateConnection();
    return;
  }

  PrintBackendServiceImpl::EnumeratePrinters(std::move(callback));
}

void PrintBackendServiceTestImpl::GetDefaultPrinterName(
    mojom::PrintBackendService::GetDefaultPrinterNameCallback callback) {
  if (terminate_receiver_) {
    TerminateConnection();
    return;
  }
  PrintBackendServiceImpl::GetDefaultPrinterName(std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS)
void PrintBackendServiceTestImpl::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
        callback) {
  if (terminate_receiver_) {
    TerminateConnection();
    return;
  }

  PrintBackendServiceImpl::GetPrinterSemanticCapsAndDefaults(
      printer_name, std::move(callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void PrintBackendServiceTestImpl::FetchCapabilities(
    const std::string& printer_name,
    mojom::PrintBackendService::FetchCapabilitiesCallback callback) {
  if (terminate_receiver_) {
    TerminateConnection();
    return;
  }

  PrintBackendServiceImpl::FetchCapabilities(printer_name, std::move(callback));
}

void PrintBackendServiceTestImpl::UpdatePrintSettings(
    uint32_t context_id,
    base::DictValue job_settings,
    mojom::PrintBackendService::UpdatePrintSettingsCallback callback) {
  if (terminate_receiver_) {
    TerminateConnection();
    return;
  }

  PrintBackendServiceImpl::UpdatePrintSettings(
      context_id, std::move(job_settings), std::move(callback));
}

void PrintBackendServiceTestImpl::TerminateConnection() {
  DLOG(ERROR) << "Terminating print backend service test connection";
  receiver_.reset();
}

// static
std::unique_ptr<PrintBackendServiceTestImpl>
PrintBackendServiceTestImpl::LaunchForTesting(
    mojo::Remote<mojom::PrintBackendService>& remote,
    scoped_refptr<TestPrintBackend> backend,
    bool sandboxed) {
  mojo::PendingReceiver<mojom::PrintBackendService> receiver =
      remote.BindNewPipeAndPassReceiver();

  // Private ctor.
  auto service = base::WrapUnique(new PrintBackendServiceTestImpl(
      std::move(receiver), sandboxed, std::move(backend)));
  service->Init(/*locale=*/std::string());

  // Register this test version of print backend service to be used instead of
  // launching instances out-of-process on-demand.
  if (sandboxed) {
    PrintBackendServiceManager::GetInstance().SetServiceForTesting(&remote);
  } else {
    PrintBackendServiceManager::GetInstance().SetServiceForFallbackTesting(
        &remote);
  }

  return service;
}

}  // namespace printing
