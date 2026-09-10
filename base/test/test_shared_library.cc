// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#define NATIVE_LIBRARY_TEST_ALWAYS_EXPORT __attribute__((visibility("default")))

extern "C" {

// A test function used only to verify basic dynamic symbol resolution.
int NATIVE_LIBRARY_TEST_ALWAYS_EXPORT GetSimpleTestValue() {
  return 5;
}

}  // extern "C"
