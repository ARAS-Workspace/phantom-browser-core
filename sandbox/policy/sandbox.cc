// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/sandbox.h"

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"

#if BUILDFLAG(IS_ANDROID)
#include <unistd.h>

#include "base/android/jni_android.h"
#include "third_party/jni_zero/common_apis.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "sandbox/policy/linux/sandbox_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
#include "sandbox/mac/seatbelt.h"
#endif  // BUILDFLAG(IS_MAC)

namespace sandbox {
namespace policy {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
bool Sandbox::Initialize(sandbox::mojom::Sandbox sandbox_type,
                         SandboxLinux::PreSandboxHook hook,
                         const SandboxLinux::Options& options) {
  return SandboxLinux::GetInstance()->InitializeSandbox(
      sandbox_type, std::move(hook), options);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// static
bool Sandbox::IsProcessSandboxed() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool is_browser = !command_line->HasSwitch(switches::kProcessType);

  if (!is_browser &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox)) {
    // When running with --no-sandbox, unconditionally report the process as
    // sandboxed. This lets code write |DCHECK(IsProcessSandboxed())| and not
    // break when testing with the --no-sandbox switch.
    return true;
  }

#if BUILDFLAG(IS_ANDROID)
  // Note that this does not check the status of the Seccomp sandbox.
  if (base::android::IsJavaAvailable()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    return jni_zero::ProcessIsIsolated(env);
  }

  // Fallback for javaless processes where JVM is not initialized.
  // Check the UID range matching Android's Process.isIsolatedUid
  // implementation.
  uid_t uid = getuid();
  uid_t app_id = uid % 100000;
  return (app_id >= 90000 && app_id <= 99999);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  int status = SandboxLinux::GetInstance()->GetStatus();
  constexpr int kLayer1Flags = SandboxLinux::Status::kSUID |
                               SandboxLinux::Status::kPIDNS |
                               SandboxLinux::Status::kUserNS;
  constexpr int kLayer2Flags =
      SandboxLinux::Status::kSeccompBPF | SandboxLinux::Status::kSeccompTSYNC;
  return (status & kLayer1Flags) != 0 && (status & kLayer2Flags) != 0;
#elif BUILDFLAG(IS_MAC)
  return Seatbelt::IsSandboxed();
#elif BUILDFLAG(IS_IOS)
  // Process launching on iOS is only supported via BrowserEngineKit which
  // will automatically sandbox processes.
  return !is_browser;
#else
  return false;
#endif
}

}  // namespace policy
}  // namespace sandbox
