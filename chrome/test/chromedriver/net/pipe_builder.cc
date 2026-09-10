// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/test/chromedriver/net/pipe_builder.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/pipe_connection.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"


namespace {
#if BUILDFLAG(IS_POSIX)
// The values for kReadFD and kWriteFD come from
// content/browser/devtools/devtools_pipe_handler.cc
constexpr int kReadFD = 3;
constexpr int kWriteFD = 4;
#endif
}  // namespace

const char PipeBuilder::kAsciizProtocolMode[] = "asciiz";
const char PipeBuilder::kCborProtocolMode[] = "cbor";

bool PipeBuilder::PlatformIsSupported() {
#if BUILDFLAG(IS_POSIX)
  return true;
#else
  return false;
#endif
}

PipeBuilder::PipeBuilder() = default;

PipeBuilder::~PipeBuilder() = default;

std::unique_ptr<SyncWebSocket> PipeBuilder::TakeSocket() {
  return std::unique_ptr<SyncWebSocket>(connection_.release());
}

void PipeBuilder::SetProtocolMode(std::string mode) {
  if (mode.empty()) {
    mode = kAsciizProtocolMode;
  }
  protocol_mode_ = mode;
}

Status PipeBuilder::BuildSocket() {
  if (protocol_mode_ != kAsciizProtocolMode) {
    return Status{kUnknownError, "only ASCIIZ protocol mode is supported"};
  }
#if BUILDFLAG(IS_POSIX)
  if (!read_file_.is_valid() || !write_file_.is_valid()) {
    return Status{kUnknownError, "pipes are not initialized"};
  }
  connection_ = std::make_unique<PipeConnection>(std::move(read_file_),
                                                 std::move(write_file_));
  return Status{kOk};
#else
  return Status{kUnknownError, "pipes are not supported on this platform"};
#endif
}

Status PipeBuilder::CloseChildEndpoints() {
#if BUILDFLAG(IS_POSIX)
  for (base::ScopedPlatformFile& file : child_ends_) {
    file = base::ScopedPlatformFile();
  }
  return Status{kOk};
#else
  return Status{kUnknownError, "pipes are not supported on this platform"};
#endif
}

Status PipeBuilder::SetUpPipes(base::LaunchOptions* options,
                               base::CommandLine* command) {
  if (protocol_mode_ != kAsciizProtocolMode) {
    return Status{kUnknownError, "only ASCIIZ protocol mode is supported"};
  }
#if BUILDFLAG(IS_POSIX)
  base::ScopedFD parent_read;
  base::ScopedFD child_write;
  base::ScopedFD child_read;
  base::ScopedFD parent_write;

  if (!CreatePipe(&parent_read, &child_write, false) ||
      !CreatePipe(&child_read, &parent_write, false) ||
      // the local ends must be closed in the child process
      !base::SetCloseOnExec(parent_read.get()) ||
      !base::SetCloseOnExec(parent_write.get())) {
    return Status{kUnknownError, "unable to setup a pipe"};
  }

  options->fds_to_remap.emplace_back(child_read.get(), kReadFD);
  options->fds_to_remap.emplace_back(child_write.get(), kWriteFD);

  read_file_ = std::move(parent_read);
  write_file_ = std::move(parent_write);
  child_ends_[0] = std::move(child_read);
  child_ends_[1] = std::move(child_write);

  return Status{kOk};
#else
  return Status{kUnknownError, "pipes are not supported on this platform"};
#endif
}
