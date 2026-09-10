// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/file_system_id.h"

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/sqlite_vfs/file_type.h"
#include "components/sqlite_vfs/metrics_util.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/stat.h>
#endif

namespace sqlite_vfs {

std::optional<FileSystemId> GetFileSystemId(Client client,
                                            const base::File& file) {
  std::optional<base::File::Error> error;

  if (file.IsValid()) {
#if BUILDFLAG(IS_POSIX)
    base::stat_wrapper_t stat_info;
    if (base::File::Fstat(file.GetPlatformFile(), &stat_info) != 0) {
      error = base::File::GetLastFileError();
    } else if (stat_info.st_ino != 0) {
      return FileSystemId{
          .dev = static_cast<dev_t>(stat_info.st_dev),
          .ino = static_cast<ino_t>(stat_info.st_ino),
      };
    }
#endif
  } else {
    error = base::File::FILE_ERROR_FAILED;
  }

  // Record `FILE_OK` if the OS returned a degenerate identifier.
  base::UmaHistogramExactLinear(
      GetHistogramName(client, "GetFileSystemIdError"),
      -error.value_or(base::File::FILE_OK), -base::File::FILE_ERROR_MAX);
  return std::nullopt;
}

}  // namespace sqlite_vfs
