// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/transferable_socket.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/tcp_socket.h"
#include "services/network/public/mojom/transferable_socket.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using TransferableSocketTest = testing::Test;

TEST_F(TransferableSocketTest, MojoTraits) {
  std::unique_ptr<net::TCPSocket> socket =
      net::TCPSocket::Create(nullptr, nullptr, net::NetLogSource());
  socket->Open(net::AddressFamily::ADDRESS_FAMILY_IPV4);
  auto socket_desc = socket->ReleaseSocketDescriptorForTesting();
  TransferableSocket transferable(socket_desc
  );
  TransferableSocket roundtripped;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TransferableSocket>(
      transferable, roundtripped));
  net::SocketDescriptor s = roundtripped.TakeSocket();
  ASSERT_NE(s, net::kInvalidSocket);
}

TEST_F(TransferableSocketTest, InvalidSocketMojoTraits) {
  auto socket_desc = net::kInvalidSocket;
  TransferableSocket transferable(socket_desc
  );
  TransferableSocket roundtripped;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TransferableSocket>(
      transferable, roundtripped));
  net::SocketDescriptor s = roundtripped.TakeSocket();
  ASSERT_EQ(s, net::kInvalidSocket);
}

TEST_F(TransferableSocketTest, EmptyMojoTraits) {
  TransferableSocket transferable;
  TransferableSocket roundtripped;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TransferableSocket>(
      transferable, roundtripped));
  net::SocketDescriptor s = roundtripped.TakeSocket();
  ASSERT_EQ(s, net::kInvalidSocket);
}

}  // namespace
}  // namespace network
