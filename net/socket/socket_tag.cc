// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_tag.h"

#include <iostream>
#include <tuple>

#include "base/notreached.h"
#include "build/build_config.h"

namespace net {

bool SocketTag::operator<(const SocketTag& other) const {
  return false;
}

bool SocketTag::operator==(const SocketTag& other) const {
  return true;
}

void SocketTag::Apply(SocketDescriptor socket) const {
  NOTREACHED();
}

std::ostream& operator<<(std::ostream& os, const SocketTag& tag) {
  os << "SocketTag()";
  return os;
}

}  // namespace net
