// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/native_messaging/native_messaging_pipe.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/webauthn/remote_webauthn_caller_security_utils.h"
#include "remoting/host/webauthn/remote_webauthn_native_messaging_host.h"

#if BUILDFLAG(IS_LINUX)
#include "remoting/base/crash/crash_reporting_crashpad.h"
#endif  // BUILDFLAG(IS_LINUX)

namespace remoting {

int RemoteWebAuthnMain(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::SingleThreadTaskExecutor task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::Create("RemoteWebAuthn");
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();

  base::CommandLine::Init(argc, argv);
  InitHostLogging();

#if defined(REMOTING_ENABLE_CRASH_REPORTING)
  if (IsUsageStatsAllowed()) {
#if BUILDFLAG(IS_LINUX)
    InitializeCrashpadReporting();
#endif  // BUILDFLAG(IS_LINUX)
  }
#endif  // defined(REMOTING_ENABLE_CRASH_REPORTING)

  if (!IsLaunchedByTrustedProcess()) {
    LOG(ERROR) << "Current process is not launched by a trusted process.";
    return kNoPermissionExitCode;
  }

  mojo::core::Init();
  mojo::core::ScopedIPCSupport ipc_support(
      task_runner, mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  base::File read_file;
  base::File write_file;

#if BUILDFLAG(IS_POSIX)
  PipeMessagingChannel::OpenAndBlockStdio(read_file, write_file);
#endif

  base::RunLoop run_loop;

  NativeMessagingPipe native_messaging_pipe;
  auto channel = std::make_unique<PipeMessagingChannel>(std::move(read_file),
                                                        std::move(write_file));

  auto native_messaging_host =
      std::make_unique<RemoteWebAuthnNativeMessagingHost>(
          base::MakeRefCounted<AutoThreadTaskRunner>(task_runner,
                                                     run_loop.QuitClosure()));
  native_messaging_host->Start(&native_messaging_pipe);
  native_messaging_pipe.Start(std::move(native_messaging_host),
                              std::move(channel));

  run_loop.Run();

  // Block until tasks blocking shutdown have completed their execution.
  base::ThreadPoolInstance::Get()->Shutdown();

  return kSuccessExitCode;
}

}  // namespace remoting
