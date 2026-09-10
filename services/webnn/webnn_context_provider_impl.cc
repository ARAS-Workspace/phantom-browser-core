// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include <memory>
#include <utility>

#include "base/byte_size.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/gpu_task_scheduler.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/features.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom-forward.h"
#include "services/webnn/webnn_context_impl.h"


#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "services/webnn/coreml/context_impl_coreml.h"  // nogncheck
#endif

#if BUILDFLAG(WEBNN_USE_LITERT)
#include "services/webnn/tflite/context_impl_litert.h"  // nogncheck
#endif

#if BUILDFLAG(WEBNN_USE_CHROME_ML_API)
#include "services/on_device_model/ml/chrome_ml.h"      // nogncheck
#include "services/on_device_model/ml/chrome_ml_api.h"  // nogncheck
#endif

#if defined(ADDRESS_SANITIZER)
#include <sanitizer/asan_interface.h>

#include "base/debug/asan_service.h"
#endif

namespace webnn {

namespace {

#if BUILDFLAG(WEBNN_USE_LITERT)
// Whether to use mojo data pipe for transferring tensor data between processes.
BASE_FEATURE(kWebNNUseDataPipe, base::FEATURE_ENABLED_BY_DEFAULT);

struct TensorDataPipes {
  mojo::ScopedDataPipeProducerHandle write_producer;
  mojo::ScopedDataPipeConsumerHandle write_consumer;
  mojo::ScopedDataPipeProducerHandle read_producer;
  mojo::ScopedDataPipeConsumerHandle read_consumer;
};

TensorDataPipes CreateTensorDataPipes() {
  TensorDataPipes pipes;
  if (base::FeatureList::IsEnabled(kWebNNUseDataPipe)) {
    constexpr base::ByteSize kDataPipeSize = base::MiBU(16);
    MojoResult result = mojo::CreateDataPipe(
        kDataPipeSize.InBytes(), pipes.write_producer, pipes.write_consumer);
    if (result != MOJO_RESULT_OK) {
      LOG(WARNING) << "Failed to create a mojo data pipe for WriteTensor.";
    }
    result = mojo::CreateDataPipe(kDataPipeSize.InBytes(), pipes.read_producer,
                                  pipes.read_consumer);
    if (result != MOJO_RESULT_OK) {
      LOG(WARNING) << "Failed to create a mojo data pipe for ReadTensor.";
    }
  }
  return pipes;
}
#endif  // BUILDFLAG(WEBNN_USE_LITERT)

WebNNContextProviderImpl::BackendForTesting* g_backend_for_testing = nullptr;

using webnn::mojom::CreateContextOptionsPtr;
using webnn::mojom::WebNNContextProvider;

// These values are persisted to logs. Entries should not be renumbered or
// removed and numeric values should never be reused.
// Please keep in sync with DeviceTypeUma in
// //tools/metrics/histograms/metadata/webnn/enums.xml.
enum class DeviceTypeUma {
  kCpu = 0,
  kGpu = 1,
  kNpu = 2,
  kMaxValue = kNpu,
};

void RecordDeviceType(const mojom::Device device) {
  DeviceTypeUma uma_value;
  switch (device) {
    case mojom::Device::kCpu:
      uma_value = DeviceTypeUma::kCpu;
      break;
    case mojom::Device::kGpu:
      uma_value = DeviceTypeUma::kGpu;
      break;
    case mojom::Device::kNpu:
      uma_value = DeviceTypeUma::kNpu;
      break;
  }
  base::UmaHistogramEnumeration("WebNN.DeviceType", uma_value);
}

#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
// Returns true if the request described by `options` should be served by the
// renderer-process (in-process) TFLite/LiteRT backend instead of the
// GPU-process backend. For `kGpu` device requests, the GPU process attempts
// execution first using the LiteRT WebGPU accelerator
// (`libLiteRtWebGpuAccelerator`, which is preloaded during
// `PreSandboxWebNNInitialization()`). If no GPU accelerator is available or if
// the request is for `kCpu` / `kNpu`, it falls back to the renderer-process
// (in-process) TFLite/LiteRT backend.
bool ShouldUseInProcessTflite(const mojom::CreateContextOptions& options) {
  return options.device != mojom::Device::kGpu;
}

void FallbackInProcessTFLite(
    WebNNContextProvider::CreateWebNNContextCallback callback) {
  std::move(callback).Run(ToError<mojom::CreateContextResult>(
      mojom::Error::Code::kFallbackToInProcess,
      "Falling back to in-process TFLite/LiteRT backend."));
}
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

#if defined(ADDRESS_SANITIZER)
NO_SANITIZE("address")
void AsanUnsafeFeatureWarning(const char* reason,
                              bool* should_exit_cleanly,
                              bool* should_abort) {
  auto* asan_service = base::debug::AsanService::GetInstance();
  asan_service->Log("\nUnsafe feature: WebMachineLearningNeuralNetwork");
}
#endif


}  // namespace

WebNNContextProviderImpl::WebNNContextProviderImpl(
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor,
    LoseAllContextsCallback lose_all_contexts_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gpu::Scheduler* scheduler,
    mojo::SharedRemote<viz::mojom::GpuHost> gpu_host)
    : gpu_feature_info_(std::move(gpu_feature_info)),
      gpu_info_(std::move(gpu_info)),
      shared_image_manager_(shared_image_manager),
      lose_all_contexts_callback_(std::move(lose_all_contexts_callback)),
      scheduler_(scheduler),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      peak_memory_monitor_(std::move(peak_memory_monitor)),
      gpu_host_(std::move(gpu_host)) {
  CHECK_NE(scheduler_, nullptr);
  CHECK_NE(main_thread_task_runner_, nullptr);
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  // `gpu_host_` is used to ensure that the execution providers used by the ORT
  // backend are ready. It should be connected to the browser process.
  CHECK(gpu_host_.is_bound());

#if defined(ADDRESS_SANITIZER)
  LOG(ERROR) << "WebMachineLearningNeuralNetwork is an unsafe feature.";
  base::debug::AsanService::GetInstance()->AddErrorCallback(
      AsanUnsafeFeatureWarning);
#endif
}

WebNNContextProviderImpl::~WebNNContextProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Destroy all GPU sequences on the main thread.
  // gpu::Scheduler will DCHECK if any sequences remain alive at destruction.
  for (const auto& [context_handle, sequence_id] : sequences_) {
    scheduler_->DestroySequence(sequence_id);
  }

  // Sequences for contexts which failed to be created and dropped the posted
  // reply must also be destroyed.
  for (const gpu::SequenceId sequence_id : pending_sequences_) {
    scheduler_->DestroySequence(sequence_id);
  }
}

std::unique_ptr<WebNNContextProviderImpl> WebNNContextProviderImpl::Create(
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor,
    LoseAllContextsCallback lose_all_contexts_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gpu::Scheduler* scheduler,
    mojo::SharedRemote<viz::mojom::GpuHost> gpu_host) {
  return base::WrapUnique(new WebNNContextProviderImpl(
      std::move(gpu_feature_info), std::move(gpu_info), shared_image_manager,
      std::move(peak_memory_monitor), std::move(lose_all_contexts_callback),
      std::move(main_thread_task_runner), scheduler, std::move(gpu_host)));
}

void WebNNContextProviderImpl::BindWebNNContextProvider(
    mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
    const WebNNReceiversParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  provider_receivers_.Add(this, std::move(receiver), params);
}

void WebNNContextProviderImpl::SetDisconnectHandlerForTesting(  // IN-TEST
    base::RepeatingClosure handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  provider_receivers_.set_disconnect_handler(std::move(handler));
}

size_t WebNNContextProviderImpl::GetContextCountForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  return context_impls_.size();
}

std::vector<std::string_view>
WebNNContextProviderImpl::GetContextBackendNamesForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::vector<std::string_view> backend_names;
  for (const auto& context_impl : context_impls_) {
    backend_names.push_back(context_impl->GetBackendName());
  }
  return backend_names;
}

void WebNNContextProviderImpl::BindWebNNServiceIntrospection(
    mojo::PendingReceiver<mojom::WebNNServiceIntrospection> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  service_introspection_receiver_.Bind(std::move(receiver));
}

void WebNNContextProviderImpl::SetClient(
    mojo::PendingRemote<mojom::WebNNServiceIntrospectionClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  service_introspection_client_.Bind(std::move(client));
}

std::vector<mojom::WebNNContextIntrospectionDetailsPtr>
WebNNContextProviderImpl::PopulateContextsDetailsForIntrospection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::vector<mojom::WebNNContextIntrospectionDetailsPtr> contexts_details;
  for (auto& context_impl : context_impls_) {
    auto details = mojom::WebNNContextIntrospectionDetails::New();
    details->context_id = context_impl->tracing_id();
    details->context_backend = context_impl->GetBackendName();
    details->execution_providers = context_impl->GetExecutionProvidersInfo();
    contexts_details.push_back(std::move(details));
  }
  return contexts_details;
}

void WebNNContextProviderImpl::GetExistingContextsDetails(
    GetExistingContextsDetailsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto contexts_details = PopulateContextsDetailsForIntrospection();
  std::move(callback).Run(std::move(contexts_details));
}

void WebNNContextProviderImpl::GetAvailableExecutionProvidersDetails(
    GetAvailableExecutionProvidersDetailsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::move(callback).Run({});
}

void WebNNContextProviderImpl::UpdateWebNNServiceIntrospection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!service_introspection_client_.is_bound()) {
    return;
  }
  auto contexts_details = PopulateContextsDetailsForIntrospection();
  service_introspection_client_->OnUpdateExistingContextDetails(
      std::move(contexts_details));

}

void WebNNContextProviderImpl::RemoveWebNNContextImpl(
    const blink::WebNNContextToken& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto context_it = context_impls_.find(handle);
  CHECK(context_it != context_impls_.end());
  context_impls_.erase(context_it);
  UpdateWebNNServiceIntrospection();
}

void WebNNContextProviderImpl::DestroyAndRemoveGpuSequence(
    const blink::WebNNContextToken& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto sequence_it = sequences_.find(handle);
  CHECK(sequence_it != sequences_.end());
  scheduler_->DestroySequence(sequence_it->second);
  sequences_.erase(sequence_it);
}


// static
void WebNNContextProviderImpl::SetBackendForTesting(
    BackendForTesting* backend_for_testing) {
  g_backend_for_testing = backend_for_testing;
}

// static
bool WebNNContextProviderImpl::HasBackendForTesting() {
  return g_backend_for_testing != nullptr;
}

void WebNNContextProviderImpl::CreateWebNNContext(
    CreateContextOptionsPtr options,
    CreateWebNNContextCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // `current_context()` must only be called within the stack frame of an actual
  // interface method invocation or disconnect notification scheduled by a
  // receiver. It is illegal to attempt to call this at any other time, such as
  // from within an asynchronous task or callback posted from a message handler.
  const WebNNReceiversParams params = provider_receivers_.current_context();

  // Force context creation to fail if the WebNN GPU feature is disabled, which
  // happens when the GPU process has crashed too many times.
  if (gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_WEBNN] ==
      gpu::kGpuFeatureStatusDisabled) {
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kUnknownError,
        "WebNN is disabled due to some unresolvable issues."));
    return;
  }

  // Generates unique IDs for WebNNContextImpl.
  static base::AtomicSequenceNumber g_next_route_id;

  // WebNN IPC operations without a SyncToken are re-posted to the scheduled
  // task runner to ensure they execute in the same sequence and order as those
  // with a SyncToken.
  const gpu::CommandBufferId command_buffer_id =
      gpu::CommandBufferIdFromChannelAndRoute(params.client_id,
                                              g_next_route_id.GetNext());

  bool use_main_thread = (g_backend_for_testing != nullptr);

#if BUILDFLAG(IS_APPLE)
  bool should_create_coreml_context = false;
  if (__builtin_available(macOS 14.4, *)) {
    should_create_coreml_context =
        base::FeatureList::IsEnabled(mojom::features::kWebNNCoreML) &&
        !params.is_incognito
#if BUILDFLAG(IS_MAC)
        && base::mac::GetCPUType() == base::mac::CPUType::kArm
#endif  // BUILDFLAG(IS_MAC)
        ;
    // CoreML contexts are created and owned on the main thread.
  }
  use_main_thread |= should_create_coreml_context;
#endif  // BUILDFLAG(IS_APPLE)

  // Task runner used to create the context on gpu sequence.
  // Backends that support multi-threading can use a separate task runner.
  scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner =
      use_main_thread ? main_thread_task_runner_
                      : base::ThreadPool::CreateSingleThreadTaskRunner(
                            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Each context gets a GPU sequence that must be destroyed on the GPU main
  // thread. See `sequences_` and `pending_sequences_` comments in the header
  // for the full sequence lifecycle.
  const gpu::SequenceId sequence_id = scheduler_->CreateSequence(
      gpu::SchedulingPriority::kNormal, owning_task_runner,
      gpu::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE, command_buffer_id);

  auto gpu_task_scheduler = std::make_unique<GpuTaskScheduler>(
      *scheduler_, command_buffer_id, sequence_id,
      gpu::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE);

  scoped_refptr<gpu::MemoryTracker> memory_tracker =
      base::MakeRefCounted<gpu::MemoryTracker>(
          command_buffer_id, params.client_tracing_id, peak_memory_monitor_,
          gpu::GpuPeakMemoryAllocationSource::WEBNN);

  ScopedTrace scoped_trace("WebNNContextProviderImpl::CreateWebNNContext");

  if (g_backend_for_testing) {
    auto [it, inserted] =
        context_impls_.emplace(g_backend_for_testing->CreateWebNNContext(
            AsWeakPtr(), std::move(options), std::move(gpu_task_scheduler),
            memory_tracker, owning_task_runner, shared_image_manager_,
            main_thread_task_runner_, std::move(callback)));
    CHECK(inserted);
    sequences_.emplace((*it)->handle(), sequence_id);
    return;
  }

  pending_sequences_.insert(sequence_id);

  WebNNContextImplPtr context_impl(nullptr,
                                   OnTaskRunnerDeleter(owning_task_runner));

  RecordDeviceType(options->device);

#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
  // Cache this before any backend (e.g. CoreML) moves `options` and
  // `context_impl` is still nullptr.
  const bool should_use_in_process_tflite = ShouldUseInProcessTflite(*options);
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)


#if BUILDFLAG(IS_APPLE)
  if (should_create_coreml_context) {
    if (__builtin_available(macOS 14.4, *)) {
      mojo::PendingRemote<mojom::WebNNContext> remote;
      auto receiver = remote.InitWithNewPipeAndPassReceiver();
      context_impl = coreml::ContextImplCoreml::Create(
          std::move(receiver), AsWeakPtr(), std::move(options),
          std::move(gpu_task_scheduler), memory_tracker, owning_task_runner,
          shared_image_manager_, main_thread_task_runner_);
      // Using mojo data pipe is not yet implemented in CoreML backend.
      OnCreateWebNNContextImpl(std::move(callback), std::move(remote),
                               mojo::ScopedDataPipeProducerHandle(),
                               mojo::ScopedDataPipeConsumerHandle(),
                               sequence_id, command_buffer_id,
                               std::move(context_impl));
      return;
    }
  }
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(WEBNN_USE_LITERT)
  // Attempt to create a LiteRT GPU context (`ContextImplLiteRt`) in the GPU
  // process using the WebGPU accelerator preloaded prior to sandbox lockdown
  // (`PreSandboxWebNNInitialization()`). If context creation fails or is not
  // supported, returning `kNotSupportedError` from `OnCreateWebNNContextImpl`
  // lets the renderer's `ML::createContext` fallback path create the in-process
  // LiteRT context instead.
  if (!context_impl && !should_use_in_process_tflite) {
    CreateLiteRtContext(std::move(scoped_trace), std::move(options),
                        std::move(gpu_task_scheduler),
                        std::move(owning_task_runner), std::move(callback),
                        params.is_incognito, memory_tracker);
    return;
  }
#endif  // BUILDFLAG(WEBNN_USE_LITERT)


#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
  // No GPU-process backend was selected and the request should be served by
  // the renderer-process in-process TFLite backend.
  if (!context_impl && should_use_in_process_tflite) {
    FallbackInProcessTFLite(std::move(callback));
    return;
  }
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

  OnCreateWebNNContextImpl(std::move(callback),
                           mojo::PendingRemote<mojom::WebNNContext>(),
                           mojo::ScopedDataPipeProducerHandle(),
                           mojo::ScopedDataPipeConsumerHandle(), sequence_id,
                           command_buffer_id, std::move(context_impl));
}

void WebNNContextProviderImpl::OnCreateWebNNContextImpl(
    CreateWebNNContextCallback callback,
    mojo::PendingRemote<::webnn::mojom::WebNNContext> remote,
    mojo::ScopedDataPipeProducerHandle write_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
    gpu::SequenceId sequence_id,
    gpu::CommandBufferId command_buffer_id,
    WebNNContextImplPtr context_impl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Remove from the pending set now that the reply has arrived.
  // This is a no-op for synchronous callers that never inserted.
  pending_sequences_.erase(sequence_id);

  if (!context_impl) {
    scheduler_->DestroySequence(sequence_id);
    WebNNContextImpl::RecordContextBackendUma(
        WebNNContextImpl::ContextBackendUma::kNotSupported);
    // TODO(crbug.com/40206287): Supporting WebNN on the platform.
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kNotSupportedError,
        "WebNN is not supported on this platform."));
    LOG(ERROR) << "WebNN is not supported on this platform.";
    return;
  }

  ContextProperties context_properties = context_impl->properties();
  const blink::WebNNContextToken& context_handle = context_impl->handle();

  sequences_.emplace(context_handle, sequence_id);
  context_impls_.emplace(std::move(context_impl));

  UpdateWebNNServiceIntrospection();

  auto success = mojom::CreateContextSuccess::New(
      std::move(remote), /*compiler_context_remote=*/mojo::NullRemote(),
      std::move(context_properties), std::move(context_handle),
      std::move(write_tensor_producer), std::move(read_tensor_consumer),
      command_buffer_id.GetUnsafeValue());
  std::move(callback).Run(
      mojom::CreateContextResult::NewSuccess(std::move(success)));
}

void WebNNContextProviderImpl::CreateWeightsFile(
    viz::mojom::GpuHost::CreateWebNNWeightsFileCallback callback) {
  gpu_host_->CreateWebNNWeightsFile(std::move(callback));
}



#if BUILDFLAG(WEBNN_USE_LITERT)
void WebNNContextProviderImpl::CreateLiteRtContext(
    ScopedTrace scoped_trace,
    mojom::CreateContextOptionsPtr options,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker) {
  const gpu::SequenceId sequence_id = gpu_task_scheduler->sequence_id();
  const gpu::CommandBufferId command_buffer_id =
      gpu_task_scheduler->command_buffer_id();
  mojo::PendingRemote<mojom::WebNNContext> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();
  TensorDataPipes pipes = CreateTensorDataPipes();
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &litert::ContextImplLiteRt::Create, std::move(receiver), AsWeakPtr(),
          std::move(options), std::move(pipes.write_consumer),
          std::move(pipes.read_producer), std::move(gpu_task_scheduler),
          std::move(memory_tracker), task_runner,
          base::Unretained(shared_image_manager_.get()),
          main_thread_task_runner_, std::move(scoped_trace), is_incognito),
      base::BindOnce(&WebNNContextProviderImpl::OnCreateWebNNContextImpl,
                     AsWeakPtr(), std::move(callback), std::move(remote),
                     std::move(pipes.write_producer),
                     std::move(pipes.read_consumer), sequence_id,
                     command_buffer_id));
}
#endif  // BUILDFLAG(WEBNN_USE_LITERT)


}  // namespace webnn
