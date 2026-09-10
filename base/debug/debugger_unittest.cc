// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"

#include "build/build_config.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
void CrashWithBreakDebugger() {
  base::debug::SetSuppressDebugUI(false);
  base::debug::BreakDebugger();

}
#endif  // defined(GTEST_HAS_DEATH_TEST)

}  // namespace

// Death tests misbehave on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

TEST(Debugger, CrashAtBreakpoint) {
  EXPECT_DEATH(CrashWithBreakDebugger(), "");
}

#else   // defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
TEST(Debugger, NoTest) {}
#endif  // defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
