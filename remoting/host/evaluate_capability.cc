// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/evaluate_capability.h"

#include <iostream>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/ipc_constants.h"

namespace remoting {

// TODO(crbug.com/40155401): Do not perform blocking operations on the IO
// thread.
class ScopedBypassIOThreadRestrictions : public base::ScopedAllowBlocking {};

namespace {

// Returns the full path of the binary file we should use to evaluate the
// capability. According to the platform and executing environment, return of
// this function may vary. But in one process, the return value is guaranteed to
// be the same.
// This function uses capability_test_stub in unittest, or tries to use current
// binary if supported, otherwise it falls back to use the default binary.
base::FilePath BuildHostBinaryPath() {
  base::FilePath path;
  bool result = base::PathService::Get(base::FILE_EXE, &path);
  DCHECK(result);
  base::FilePath directory;
  result = base::PathService::Get(base::DIR_EXE, &directory);
  DCHECK(result);
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_unittests")) {
    return directory.Append(FILE_PATH_LITERAL("capability_test_stub"));
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (path.BaseName().value() ==
      FILE_PATH_LITERAL("chrome-remote-desktop-host")) {
    return path;
  }
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_me2me_host")) {
    return path;
  }

  return directory.Append(FILE_PATH_LITERAL("remoting_me2me_host"));
#elif BUILDFLAG(IS_APPLE)
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_me2me_host")) {
    return path;
  }

  return directory.Append(FILE_PATH_LITERAL(
      "remoting_me2me_host.app/Contents/MacOS/remoting_me2me_host"));
#else
#error "BuildHostBinaryPath is not implemented for current platform."
#endif
}

}  // namespace

int EvaluateCapabilityLocally(const std::string& type) {

  return kInvalidCommandLineExitCode;
}

int EvaluateCapability(const std::string& type,
                       std::string* output /* = nullptr */) {
  base::CommandLine command(BuildHostBinaryPath());
  command.AppendSwitchASCII(kProcessTypeSwitchName,
                            kProcessTypeEvaluateCapability);
  command.AppendSwitchASCII(kEvaluateCapabilitySwitchName, type);

  int exit_code;
  std::string dummy_output;
  if (!output) {
    output = &dummy_output;
  }

  // TODO(crbug.com/40155401): Do not perform blocking operations on the IO
  // thread.
  ScopedBypassIOThreadRestrictions bypass;
#if DCHECK_IS_ON()
  const bool result =
#endif
      base::GetAppOutputWithExitCode(command, output, &exit_code);

#if DCHECK_IS_ON()
  DCHECK(result) << "Failed to execute process "
                 << command.GetCommandLineString() << ", exit code "
                 << exit_code;
#endif

  return exit_code;
}

}  // namespace remoting
