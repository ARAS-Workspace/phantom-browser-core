// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/environment_internal.h"

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using EnvironmentInternalTest = PlatformTest;

namespace base::internal {

TEST_F(EnvironmentInternalTest, AlterEnvironment) {
  // SAFETY: The environment blocks used in these tests are static and
  // null-terminated, satisfying AlterEnvironment's requirements.
  auto empty = base::span<const char* const>();
  auto a2 = std::vector<const char*>{"A=2"};
  auto a2b3 = std::vector<const char*>{"A=2", "B=3"};
  EnvironmentMap changes;
  base::HeapArray<char*> e;

  e = UNSAFE_BUFFERS(AlterEnvironment(empty, changes));
  EXPECT_TRUE(e[0] == nullptr);

  changes["A"] = "1";
  e = UNSAFE_BUFFERS(AlterEnvironment(empty, changes));
  EXPECT_EQ(std::string("A=1"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  e = UNSAFE_BUFFERS(AlterEnvironment(empty, changes));
  EXPECT_TRUE(e[0] == nullptr);

  changes.clear();
  e = UNSAFE_BUFFERS(AlterEnvironment(a2, changes));
  EXPECT_EQ(std::string("A=2"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = "1";
  e = UNSAFE_BUFFERS(AlterEnvironment(a2, changes));
  EXPECT_EQ(std::string("A=1"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  e = UNSAFE_BUFFERS(AlterEnvironment(a2, changes));
  EXPECT_TRUE(e[0] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  changes["B"] = std::string();
  e = UNSAFE_BUFFERS(AlterEnvironment(a2b3, changes));
  EXPECT_TRUE(e[0] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  e = UNSAFE_BUFFERS(AlterEnvironment(a2b3, changes));
  EXPECT_EQ(std::string("B=3"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["B"] = std::string();
  e = UNSAFE_BUFFERS(AlterEnvironment(a2b3, changes));
  EXPECT_EQ(std::string("A=2"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = "1";
  changes["C"] = "4";
  e = UNSAFE_BUFFERS(AlterEnvironment(a2b3, changes));
  EXPECT_EQ(std::string("B=3"), e[0]);
  // AlterEnvironment() currently always puts changed entries at the end.
  EXPECT_EQ(std::string("A=1"), e[1]);
  EXPECT_EQ(std::string("C=4"), e[2]);
  EXPECT_TRUE(e[3] == nullptr);
}

}  // namespace base::internal
