// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line_rust_shim.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace base::rust {

std::unique_ptr<base::CommandLine> NewForProgram(::rust::Str program) {
  std::string sprogram(program);
  return std::make_unique<base::CommandLine>(base::FilePath(sprogram));
}

bool HasSwitch(const base::CommandLine& cmd, ::rust::Str switch_string) {
  return cmd.HasSwitch(std::string_view{switch_string});
}

::rust::String GetSwitchValueASCII(const base::CommandLine& cmd,
                                   ::rust::Str switch_string) {
  return ::rust::String(
      cmd.GetSwitchValueASCII(std::string_view{switch_string}));
}

void AppendSwitch(base::CommandLine& cmd, ::rust::Str switch_string) {
  cmd.AppendSwitch(std::string_view{switch_string});
}

void AppendSwitchASCII(base::CommandLine& cmd,
                       ::rust::Str switch_string,
                       ::rust::Str value) {
  cmd.AppendSwitchASCII(std::string_view{switch_string},
                        std::string_view{value});
}

void RemoveSwitch(base::CommandLine& cmd, ::rust::Str switch_string) {
  cmd.RemoveSwitch(std::string_view{switch_string});
}

void AppendArg(base::CommandLine& cmd, ::rust::Str value) {
  cmd.AppendArg(std::string_view{value});
}

::rust::Vec<::rust::String> GetArgs(const base::CommandLine& cmd) {
  ::rust::Vec<::rust::String> result;
  for (const auto& arg : cmd.GetArgs()) {
    result.push_back(::rust::String(arg));
  }
  return result;
}

::rust::String GetProgram(const base::CommandLine& cmd) {
  return ::rust::String(cmd.GetProgram().value());
}

bool GetAppOutput(const base::CommandLine& cmd, ::rust::String& output) {
  std::string std_output;
  bool result = base::GetAppOutput(cmd, &std_output);
  output = ::rust::String(std_output);
  return result;
}

}  // namespace base::rust
