// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources_util.h"

#include <stddef.h>

#include <array>

#include "build/build_config.h"
#include "components/grit/components_scaled_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/resources/grit/ui_resources.h"

TEST(ResourcesUtil, SpotCheckIds) {
  struct Cases {
    const char* name;
    int id;
  };
  const auto kCases = std::to_array<Cases>({
      // IDRs from chrome/app/theme/theme_resources.grd should be valid.
      {"IDR_ERROR_NETWORK_GENERIC", IDR_ERROR_NETWORK_GENERIC},
      // IDRs from ui/resources/ui_resources.grd should be valid.
      {"IDR_DEFAULT_FAVICON", IDR_DEFAULT_FAVICON},
      // Unknown names should be invalid and return -1.
      {"foobar", -1},
      {"backstar", -1},
  });

  for (size_t i = 0; i < std::size(kCases); ++i)
    EXPECT_EQ(kCases[i].id, ResourcesUtil::GetThemeResourceId(kCases[i].name));
}
