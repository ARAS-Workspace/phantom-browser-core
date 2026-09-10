// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/shell_crash_reporter_client.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/shell/common/shell_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/shell/android/shell_descriptors.h"
#endif

namespace content {

namespace {

base::FilePath GetCrashDumpLocationInternal() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kCrashDumpsDir)) {
    return command_line->GetSwitchValuePath(switches::kCrashDumpsDir);
  }
  base::FilePath default_dir;
#if BUILDFLAG(IS_IOS)
  CHECK(base::PathService::Get(base::DIR_CACHE, &default_dir));
  default_dir = default_dir.Append("Crashpad");
#endif  // BUILDFLAG(IS_IOS)
  return default_dir;
}

}  // namespace

ShellCrashReporterClient::ShellCrashReporterClient() {}
ShellCrashReporterClient::~ShellCrashReporterClient() {}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
base::FilePath ShellCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(FILE_PATH_LITERAL("uploads.log"));
}
#endif

bool ShellCrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
  base::FilePath crash_directory = GetCrashDumpLocationInternal();
  if (crash_directory.empty()) {
    return false;
  }
  *crash_dir = std::move(crash_directory);
  return true;
}

void ShellCrashReporterClient::GetProductInfo(ProductInfo* product_info) {
  product_info->product_name = "content_shell";
  product_info->version = CONTENT_SHELL_VERSION;
}

bool ShellCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kGpuProcess;
}

}  // namespace content
