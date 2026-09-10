// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_native_library.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Tests whether or not a function pointer retrieved via ScopedNativeLibrary
// is available only in a scope.
TEST(ScopedNativeLibrary, Basic) {
}

}  // namespace base
