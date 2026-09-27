// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_TAG_H_
#define NET_SOCKET_SOCKET_TAG_H_

#include <iosfwd>

#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/socket/socket_descriptor.h"

namespace net {

// SocketTag represents a tag that can be applied to a socket. Currently only
// implemented for Android, it facilitates assigning a Android TrafficStats tag
// and UID to a socket so that future network data usage by the socket is
// attributed to the tag and UID that the socket is tagged with.
//
// This class is small (<=64-bits) and contains only POD to facilitate default
// copy and assignment operators so that it can easily be passed by value.
class NET_EXPORT SocketTag {
 public:
  SocketTag() = default;
  ~SocketTag() = default;

  bool operator<(const SocketTag& other) const;
  bool operator==(const SocketTag& other) const;

  // Apply this tag to |socket|.
  void Apply(SocketDescriptor socket) const;
};

// Allows for logging of SocketTag.
NET_EXPORT std::ostream& operator<<(std::ostream& os, const SocketTag& tag);

}  // namespace net

#endif  // NET_SOCKET_SOCKET_TAG_H_
