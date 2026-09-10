// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/test_util.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

namespace remoting {

bool MakePipe(base::File* read_file, base::File* write_file) {
#if BUILDFLAG(IS_POSIX)
  int fds[2];
  if (pipe(fds) == 0) {
    *read_file = base::File(fds[0]);
    *write_file = base::File(fds[1]);
    return true;
  }
  return false;
#else
#error Not implemented
#endif
}

}  // namespace remoting
