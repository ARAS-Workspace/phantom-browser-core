// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_tag.h"

#include "build/build_config.h"

#include <stdint.h>

#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/sockaddr_storage.h"
#include "net/socket/socket_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Test that SocketTag's comparison function work.
TEST(SocketTagTest, Compares) {
  SocketTag unset1;
  SocketTag unset2;

  EXPECT_TRUE(unset1 == unset2);
  EXPECT_FALSE(unset1 != unset2);
  EXPECT_FALSE(unset1 < unset2);
}

// On Android, where socket tagging is supported, verify that SocketTag::Apply
// works as expected.

}  // namespace net
