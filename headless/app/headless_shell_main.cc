// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "headless/public/headless_shell.h"

#if BUILDFLAG(IS_MAC)
#include "base/check.h"
#include "sandbox/mac/seatbelt_exec.h"
#endif

int main(int argc, const char** argv) {
  content::ContentMainParams params(nullptr);
#if !BUILDFLAG(IS_ANDROID)
  params.argc = argc;
  params.argv = argv;
#if BUILDFLAG(IS_MAC)
  sandbox::SeatbeltExecServer::CreateFromArgumentsResult seatbelt =
      sandbox::SeatbeltExecServer::CreateFromArguments(
          argv[0], argc, const_cast<char**>(argv));
  if (seatbelt.sandbox_required) {
    CHECK(seatbelt.server->InitializeSandbox());
  }
#endif  // BUILDFLAG(IS_MAC)
#endif  // !BUILDFLAG(IS_ANDROID)

  return headless::HeadlessShellMain(std::move(params));
}
