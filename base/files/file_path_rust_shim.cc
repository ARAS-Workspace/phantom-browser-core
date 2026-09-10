// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path_rust_shim.h"

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"

namespace base::rust::file_path {

std::unique_ptr<FilePath> CreateFilePathFromBytes(
    ::rust::Slice<const uint8_t> bytes) {
  return std::make_unique<FilePath>(
      base::as_string_view(base::as_chars(base::span(bytes))));
}

::rust::Slice<const uint8_t> FilePathToBytes(const FilePath& path) {
  const auto bytes = base::as_byte_span(path.value());
  return ::rust::Slice<const uint8_t>(bytes.data(), bytes.size());
}

}  // namespace base::rust::file_path
