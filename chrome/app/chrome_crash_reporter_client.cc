// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_crash_reporter_client.h"

#include <optional>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/env_vars.h"
#include "components/version_info/version_info_values.h"

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
#include "components/version_info/version_info.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/debug/dump_without_crashing.h"
#endif

namespace {

constexpr const char* UpdaterVersion() {
  return PRODUCT_VERSION;
}

}  // namespace

void ChromeCrashReporterClient::Create() {
  static base::NoDestructor<ChromeCrashReporterClient> crash_client;
  crash_reporter::SetCrashReporterClient(crash_client.get());

  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write crash dumps can be set.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath crash_dumps_dir_path;
  std::optional<std::string> alternate_crash_dump_location =
      env->GetVar("BREAKPAD_DUMP_LOCATION");
  if (alternate_crash_dump_location.has_value()) {
    crash_dumps_dir_path =
        base::FilePath::FromUTF8Unsafe(alternate_crash_dump_location.value());
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 "breakpad-dump-location")) {
    // This is needed for Android tests, where we want dumps to go to a location
    // where they don't get uploaded/deleted, but we can't use environment
    // variables.
    crash_dumps_dir_path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            "breakpad-dump-location");
  }
  if (!crash_dumps_dir_path.empty()) {
    base::PathService::Override(chrome::DIR_CRASH_DUMPS, crash_dumps_dir_path);
  }
}

ChromeCrashReporterClient::ChromeCrashReporterClient() = default;

ChromeCrashReporterClient::~ChromeCrashReporterClient() = default;

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
bool ChromeCrashReporterClient::GetShouldDumpLargerDumps() {
  return chrome::GetChannel() != version_info::Channel::STABLE;
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

bool ChromeCrashReporterClient::GetCrashDumpLocation(
    base::FilePath* crash_dir) {
  return base::PathService::Get(chrome::DIR_CRASH_DUMPS, crash_dir);
}

void ChromeCrashReporterClient::GetProductInfo(ProductInfo* product_info) {
  CHECK(product_info);

#if BUILDFLAG(IS_LINUX)
#if defined(ADDRESS_SANITIZER)
  product_info->product_name = "Chrome_Linux_ASan";
#else
  product_info->product_name = "Chrome_Linux";
#endif  // defined(ADDRESS_SANITIZER)
#elif BUILDFLAG(IS_MAC)
  product_info->product_name = "Chrome_Mac";
#else
  NOTREACHED();
#endif

  product_info->version = UpdaterVersion();
  product_info->channel =
      chrome::GetChannelName(chrome::WithExtendedStable(true));
}

bool ChromeCrashReporterClient::IsRunningUnattended() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return env->HasVar(env_vars::kHeadless);
}
