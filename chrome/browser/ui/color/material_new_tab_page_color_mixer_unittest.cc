// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_new_tab_page_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_mixers.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"

namespace {

class MaterialNewTabPageColorMixerTest : public testing::Test {
 public:
  MaterialNewTabPageColorMixerTest() = default;

  ui::ColorProvider& color_provider() { return color_provider_; }

  void AddMaterialColorMixers() {
    AddColorMixers(&color_provider_, color_provider_key_);
    AddChromeColorMixers(&color_provider_, color_provider_key_);
  }

 private:
  ui::ColorProvider color_provider_;
  ui::ColorProviderKey color_provider_key_;
};

TEST_F(MaterialNewTabPageColorMixerTest, NewTabPageColorsUseMaterialTokens) {
  AddMaterialColorMixers();

  EXPECT_EQ(color_provider().GetColor(kColorNewTabPageBackground),
            color_provider().GetColor(ui::kColorSysBase));
  EXPECT_EQ(color_provider().GetColor(kColorNewTabPageButtonBackground),
            color_provider().GetColor(ui::kColorSysTonalContainer));
  EXPECT_EQ(
      color_provider().GetColor(kColorNewTabPageMostVisitedTileBackground),
      color_provider().GetColor(ui::kColorSysBaseContainer));
}

}  // namespace
