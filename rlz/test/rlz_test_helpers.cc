// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Main entry point for all unit tests.

#include "rlz_test_helpers.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "rlz/lib/rlz_lib.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/file_path.h"
#include "rlz/lib/rlz_value_store.h"
#endif

void RlzLibTestNoMachineStateHelper::SetUp() {
#if BUILDFLAG(IS_APPLE)
  base::apple::ScopedNSAutoreleasePool pool;
#endif  // BUILDFLAG(IS_APPLE)
#if BUILDFLAG(IS_POSIX)
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  rlz_lib::testing::SetRlzStoreDirectory(temp_dir_.GetPath());
#endif  // BUILDFLAG(IS_POSIX)
}

void RlzLibTestNoMachineStateHelper::TearDown() {
#if BUILDFLAG(IS_POSIX)
  rlz_lib::testing::SetRlzStoreDirectory(base::FilePath());
#endif  // BUILDFLAG(IS_POSIX)
}

void RlzLibTestNoMachineStateHelper::Reset() {
#if BUILDFLAG(IS_POSIX)
  ASSERT_TRUE(temp_dir_.Delete());
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  rlz_lib::testing::SetRlzStoreDirectory(temp_dir_.GetPath());
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_POSIX)
}

void RlzLibTestNoMachineState::SetUp() {
  m_rlz_test_helper_.SetUp();
}

void RlzLibTestNoMachineState::TearDown() {
  m_rlz_test_helper_.TearDown();
}

RlzLibTestBase::RlzLibTestBase() = default;

RlzLibTestBase::~RlzLibTestBase() = default;

void RlzLibTestBase::SetUp() {
  RlzLibTestNoMachineState::SetUp();

#if BUILDFLAG(IS_POSIX)
  // Make sure the values of RLZ strings for access points used in tests start
  // out not set, since on Chrome OS RLZ string can only be set once.
  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, ""));
  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IE_HOME_PAGE, ""));
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_POSIX)
  for (int p = rlz_lib::IE_TOOLBAR; p <= rlz_lib::PARTNER; p++) {
    rlz_lib::ClearAllProductEvents(static_cast<rlz_lib::Product>(p));
  }
#endif

}

void RlzLibTestBase::TearDown() {
}
