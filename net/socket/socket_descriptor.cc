// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_descriptor.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/socket.h>
#include <sys/types.h>
#endif

#if BUILDFLAG(IS_APPLE)
#include <unistd.h>
#endif

namespace net {

SocketDescriptor CreatePlatformSocket(int family, int type, int protocol) {
#if BUILDFLAG(IS_POSIX)
  SocketDescriptor result = ::socket(family, type, protocol);
  return result;
#endif  // BUILDFLAG(IS_POSIX)
}

}  // namespace net
