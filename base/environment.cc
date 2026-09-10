// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <stdlib.h>
#endif

namespace base {

namespace {

std::optional<std::string> GetVarImpl(cstring_view variable_name) {
#if BUILDFLAG(IS_POSIX)
  const char* env_value = getenv(variable_name.c_str());
  if (!env_value) {
    return std::nullopt;
  }
  return std::string(env_value);
#endif
}

}  // namespace

Environment::Environment() = default;

Environment::~Environment() = default;

// static
std::unique_ptr<Environment> Environment::Create() {
  return std::make_unique<Environment>();
}

std::optional<std::string> Environment::GetVar(cstring_view variable_name) {
  auto result = GetVarImpl(variable_name);
  if (result.has_value()) {
    return result;
  }

  // Some commonly used variable names are uppercase while others
  // are lowercase, which is inconsistent. Let's try to be helpful
  // and look for a variable name with the reverse case.
  // I.e. HTTP_PROXY may be http_proxy for some users/systems.
  char first_char = variable_name[0];
  std::string alternate_case_var;
  if (IsAsciiLower(first_char)) {
    alternate_case_var = ToUpperASCII(variable_name);
  } else if (IsAsciiUpper(first_char)) {
    alternate_case_var = ToLowerASCII(variable_name);
  } else {
    return std::nullopt;
  }
  return GetVarImpl(alternate_case_var);
}

bool Environment::SetVar(cstring_view variable_name,
                         const std::string& new_value) {
#if BUILDFLAG(IS_POSIX)
  // On success, zero is returned.
  return !setenv(variable_name.c_str(), new_value.c_str(), 1);
#endif
}

bool Environment::UnSetVar(cstring_view variable_name) {
#if BUILDFLAG(IS_POSIX)
  // On success, zero is returned.
  return !unsetenv(variable_name.c_str());
#endif
}

bool Environment::HasVar(cstring_view variable_name) {
  return GetVar(variable_name).has_value();
}

}  // namespace base
