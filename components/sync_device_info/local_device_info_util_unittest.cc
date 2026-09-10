// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/local_device_info_util.h"

#include <utility>

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

// Call GetPersonalizableDeviceNameBlocking and make sure its return
// value looks sane.
TEST(GetClientNameTest, GetPersonalizableDeviceNameBlocking) {
  const std::string& client_name = GetPersonalizableDeviceNameBlocking();
  EXPECT_FALSE(client_name.empty());
}

}  // namespace
}  // namespace syncer
