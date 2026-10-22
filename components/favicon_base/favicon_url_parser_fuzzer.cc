// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_url_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/no_destructor.h"

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static const base::NoDestructor<IcuEnvironment> env;

  const std::string string_input(reinterpret_cast<const char*>(data), size);
  chrome::ParsedFaviconPath parsed;
  chrome::ParseFaviconPath(string_input, chrome::FaviconUrlFormat::kFavicon2,
                           &parsed);
  return 0;
}
