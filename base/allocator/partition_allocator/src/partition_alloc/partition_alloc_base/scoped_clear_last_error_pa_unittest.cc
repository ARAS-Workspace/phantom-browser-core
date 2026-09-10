// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/scoped_clear_last_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal::base {

TEST(PAScopedClearLastError, TestNoError) {
  errno = 1;
  {
    ScopedClearLastError clear_error;
    EXPECT_EQ(0, errno);
  }
  EXPECT_EQ(1, errno);
}

TEST(PAScopedClearLastError, TestError) {
  errno = 1;
  {
    ScopedClearLastError clear_error;
    errno = 2;
  }
  EXPECT_EQ(1, errno);
}

}  // namespace partition_alloc::internal::base
