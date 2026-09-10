// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/remote_open_url_util.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace remoting {

bool IsRemoteOpenUrlSupported() {
#if BUILDFLAG(IS_LINUX)
  return true;
#else
  // Not supported on other platforms.
  return false;
#endif
}

}  // namespace remoting
