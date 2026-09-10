// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/me2me_native_messaging_host_main.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/gaia_oauth_client.h"
#include "remoting/base/logging.h"
#include "remoting/base/memory_consumer_registry.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/native_messaging/native_messaging_pipe.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"
#include "remoting/host/pairing_registry_delegate.h"
#include "remoting/host/setup/me2me_native_messaging_host.h"
#include "remoting/host/usage_stats_consent.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_LINUX)
#include "remoting/base/crash/crash_reporting_crashpad.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/host/pairing_registry_delegate_linux.h"
#endif  // BUILDFLAG(IS_LINUX)

#if defined(USE_GLIB) && !BUILDFLAG(IS_CHROMEOS)
#include <glib-object.h>
#endif  // defined(USE_GLIB) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
#include "remoting/host/chromeos/browser_interop.h"
#endif

using remoting::protocol::PairingRegistry;

namespace remoting {

int Me2MeNativeMessagingHostMain(int argc, char** argv) {
  base::ScopedMemoryConsumerRegistry<remoting::MemoryConsumerRegistry>
      memory_consumer_registry;

  // This object instance is required by Chrome code (such as
  // SingleThreadTaskExecutor).
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  remoting::InitHostLogging();

#if BUILDFLAG(IS_APPLE)
  // Needed so we don't leak objects when threads are created.
  base::apple::ScopedNSAutoreleasePool pool;
#endif  // BUILDFLAG(IS_APPLE)

#if defined(USE_GLIB) && !BUILDFLAG(IS_CHROMEOS)
// g_type_init will be deprecated in 2.36. 2.35 is the development
// version for 2.36, hence do not call g_type_init starting 2.35.
// http://developer.gnome.org/gobject/unstable/gobject-Type-Information.html#g-type-init
#if !GLIB_CHECK_VERSION(2, 35, 0)
  g_type_init();
#endif
#endif  // defined(USE_GLIB) && !BUILDFLAG(IS_CHROMEOS)

  // Required to find the ICU data file, used by some file_util routines.
  base::i18n::InitializeICU();

#if defined(REMOTING_ENABLE_CRASH_REPORTING)
  // Initialize crash reporting as early as possible. On Mac the command-line
  // needs to be initialized first, so that the preference for crash-reporting
  // can be looked up in the config file.
  if (IsUsageStatsAllowed()) {
#if BUILDFLAG(IS_LINUX)
    InitializeCrashpadReporting();
#endif  // BUILDFLAG(IS_LINUX)
  }
#endif  // defined(REMOTING_ENABLE_CRASH_REPORTING)

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("Me2Me");

  mojo::core::Init();

  // An IO thread is needed for the pairing registry and URL context getter.
  base::Thread io_thread("io_thread");
  io_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI,
                                                    /*is_main_thread=*/true);
  base::RunLoop run_loop;

  scoped_refptr<DaemonController> daemon_controller =
      DaemonController::Create();

#if BUILDFLAG(IS_APPLE)
  if (command_line->HasSwitch(kCheckPermissionSwitchName)) {
    int exit_code;
    daemon_controller->CheckPermission(
        /* it2me */ false,
        // base::BindOnce cannot bind a capturing lambda, so the "captured"
        // parameters are bound manually. This is safe because the run-loop is
        // run to completion within this scope.
        base::BindOnce(
            [](int* exit_code, base::RunLoop* run_loop, bool perm) {
              *exit_code = (perm ? kSuccessExitCode : kNoPermissionExitCode);
              run_loop->Quit();
            },
            &exit_code, &run_loop));
    run_loop.Run();
    return exit_code;
  }
#endif  // BUILDFLAG(IS_APPLE)

  // Pass handle of the native view to the controller so that the UAC prompts
  // are focused properly.
  int64_t native_view_handle = 0;
  if (command_line->HasSwitch(kParentWindowSwitchName)) {
    std::string native_view =
        command_line->GetSwitchValueASCII(kParentWindowSwitchName);
    if (!base::StringToInt64(native_view, &native_view_handle)) {
      LOG(WARNING) << "Invalid parameter value --" << kParentWindowSwitchName
                   << "=" << native_view;
    }
  }

  base::File read_file;
  base::File write_file;

#if BUILDFLAG(IS_POSIX)
  PipeMessagingChannel::OpenAndBlockStdio(read_file, write_file);
#else
#error Not implemented.
#endif

  // OAuth client (for credential requests). IO thread is used for blocking
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter(
      new URLRequestContextGetter(io_thread.task_runner()));
  network::TransitionalURLLoaderFactoryOwner url_loader_factory_owner(
      url_request_context_getter);
  std::unique_ptr<OAuthClient> oauth_client(
      new GaiaOAuthClient(url_loader_factory_owner.GetURLLoaderFactory()));

  // Create the pairing registry.
  scoped_refptr<PairingRegistry> pairing_registry;

#if BUILDFLAG(IS_LINUX)
  if (daemon_controller->is_multi_process()) {
    pairing_registry = base::MakeRefCounted<PairingRegistry>(
        io_thread.task_runner(),
        std::make_unique<PairingRegistryDelegateLinux>(
            GetMultiProcessHostGlobalConfigDir().Append(
                PairingRegistryDelegateLinux::kRegistryDirectory),
            /*use_unprivileged_file=*/true));
  } else {
    pairing_registry = CreatePairingRegistry(io_thread.task_runner());
  }
#else
  pairing_registry = CreatePairingRegistry(io_thread.task_runner());
#endif

  std::unique_ptr<NativeMessagingPipe> native_messaging_pipe(
      new NativeMessagingPipe());

  // Set up the native messaging channel.
  std::unique_ptr<extensions::NativeMessagingChannel> channel(
      new PipeMessagingChannel(std::move(read_file), std::move(write_file)));

  std::unique_ptr<ChromotingHostContext> context =
#if !BUILDFLAG(IS_CHROMEOS)
      ChromotingHostContext::Create(new remoting::AutoThreadTaskRunner(
          main_task_executor.task_runner(), run_loop.QuitClosure()));
#else   // !BUILDFLAG(IS_CHROMEOS)
      base::MakeRefCounted<BrowserInterop>()->CreateChromotingHostContext(
          nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Create the native messaging host.
  std::unique_ptr<extensions::NativeMessageHost> host(
      new Me2MeNativeMessagingHost(static_cast<intptr_t>(native_view_handle),
                                   std::move(context), daemon_controller,
                                   pairing_registry, std::move(oauth_client)));

  host->Start(native_messaging_pipe.get());

  native_messaging_pipe->Start(std::move(host), std::move(channel));

  // Run the loop until channel is alive.
  run_loop.Run();

  // Block until tasks blocking shutdown have completed their execution.
  base::ThreadPoolInstance::Get()->Shutdown();

  return kSuccessExitCode;
}

}  // namespace remoting
