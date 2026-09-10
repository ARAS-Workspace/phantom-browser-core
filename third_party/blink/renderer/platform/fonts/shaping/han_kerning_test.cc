// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/han_kerning.h"

#include <testing/gmock/include/gmock/gmock.h>
#include <testing/gtest/include/gtest/gtest.h>

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"

namespace blink {

Font* CreateNotoCjk() {
  return blink::test::CreateTestFont(
      AtomicString("Noto Sans CJK"),
      blink::test::BlinkWebTestsFontsTestDataPath(
          "noto/cjk/NotoSansCJKjp-Regular-subset-halt.otf"),
      16.0);
}

class HanKerningTest : public testing::Test {};

TEST_F(HanKerningTest, MayApply) {
  Font* noto_cjk = CreateNotoCjk();
  const SimpleFontData* noto_cjk_data = noto_cjk->PrimaryFont();
  EXPECT_TRUE(noto_cjk_data);
  scoped_refptr<LayoutLocale> ja =
      LayoutLocale::CreateForTesting(AtomicString("ja"));
  HanKerning::FontData ja_data(*noto_cjk_data, *ja, true);

  for (UChar32 ch = 0; ch < uchar::kMaxCodepoint; ++ch) {
    StringBuilder builder;
    builder.Append(ch);
    String text = builder.ToString();

    for (wtf_size_t i = 0; i < text.length(); ++i) {
      const HanKerning::CharType type =
          HanKerning::GetCharType(text[i], ja_data);
      if (type == HanKerning::CharType::kOpen ||
          type == HanKerning::CharType::kOpenQuote ||
          type == HanKerning::CharType::kClose ||
          type == HanKerning::CharType::kCloseQuote) {
        EXPECT_EQ(HanKerning::MayApply(text), true) << Format("U+{:06X}", ch);
        break;
      }
    }
  }
}

TEST_F(HanKerningTest, FontDataHorizontal) {
  Font* noto_cjk = CreateNotoCjk();
  const SimpleFontData* noto_cjk_data = noto_cjk->PrimaryFont();
  EXPECT_TRUE(noto_cjk_data);
  scoped_refptr<LayoutLocale> ja =
      LayoutLocale::CreateForTesting(AtomicString("ja"));
  scoped_refptr<LayoutLocale> zhs =
      LayoutLocale::CreateForTesting(AtomicString("zh-hans"));
  scoped_refptr<LayoutLocale> zht =
      LayoutLocale::CreateForTesting(AtomicString("zh-hant"));
  HanKerning::FontData ja_data(*noto_cjk_data, *ja, true);
  HanKerning::FontData zhs_data(*noto_cjk_data, *zhs, true);
  HanKerning::FontData zht_data(*noto_cjk_data, *zht, true);

  // In the Adobe's common convention:
  // * Place full stop and comma at center only for Traditional Chinese.
  // * Place colon and semicolon on the left only for Simplified Chinese.
  EXPECT_EQ(ja_data.type_for_dot, HanKerning::CharType::kClose);
  EXPECT_EQ(zhs_data.type_for_dot, HanKerning::CharType::kClose);
  EXPECT_EQ(zht_data.type_for_dot, HanKerning::CharType::kMiddle);

  EXPECT_EQ(ja_data.type_for_colon, HanKerning::CharType::kMiddle);
  EXPECT_EQ(ja_data.type_for_semicolon, HanKerning::CharType::kMiddle);
  EXPECT_EQ(zhs_data.type_for_colon, HanKerning::CharType::kClose);
  EXPECT_EQ(zhs_data.type_for_semicolon, HanKerning::CharType::kClose);
  EXPECT_EQ(zht_data.type_for_colon, HanKerning::CharType::kMiddle);
  EXPECT_EQ(zht_data.type_for_semicolon, HanKerning::CharType::kMiddle);

  // Quote characters are proportional for Japanese, fullwidth for Chinese.
  EXPECT_FALSE(ja_data.is_quote_fullwidth);
  EXPECT_TRUE(zhs_data.is_quote_fullwidth);
  EXPECT_TRUE(zht_data.is_quote_fullwidth);
}

TEST_F(HanKerningTest, FontDataVertical) {
  Font* noto_cjk = CreateNotoCjk();
  const SimpleFontData* noto_cjk_data = noto_cjk->PrimaryFont();
  EXPECT_TRUE(noto_cjk_data);
  scoped_refptr<LayoutLocale> ja =
      LayoutLocale::CreateForTesting(AtomicString("ja"));
  scoped_refptr<LayoutLocale> zhs =
      LayoutLocale::CreateForTesting(AtomicString("zh-hans"));
  scoped_refptr<LayoutLocale> zht =
      LayoutLocale::CreateForTesting(AtomicString("zh-hant"));
  HanKerning::FontData ja_data(*noto_cjk_data, *ja, false);
  HanKerning::FontData zhs_data(*noto_cjk_data, *zhs, false);
  HanKerning::FontData zht_data(*noto_cjk_data, *zht, false);

  EXPECT_EQ(ja_data.type_for_dot, HanKerning::CharType::kClose);
  EXPECT_EQ(zhs_data.type_for_dot, HanKerning::CharType::kClose);
  EXPECT_EQ(zht_data.type_for_dot, HanKerning::CharType::kMiddle);

  // In the Adobe's common convention, only colon in Japanese rotates, and all
  // other cases are upright.
  EXPECT_EQ(ja_data.type_for_colon, HanKerning::CharType::kMiddle);
  EXPECT_EQ(ja_data.type_for_semicolon, HanKerning::CharType::kOther);
  EXPECT_EQ(zhs_data.type_for_colon, HanKerning::CharType::kOther);
  EXPECT_EQ(zhs_data.type_for_semicolon, HanKerning::CharType::kOther);
  EXPECT_EQ(zht_data.type_for_colon, HanKerning::CharType::kOther);
  EXPECT_EQ(zht_data.type_for_semicolon, HanKerning::CharType::kOther);

  // Quote characters are fullwidth when vertical upright, but Japanese
  // placement is different from expected.
  EXPECT_FALSE(ja_data.is_quote_fullwidth);
  EXPECT_TRUE(zhs_data.is_quote_fullwidth);
  EXPECT_TRUE(zht_data.is_quote_fullwidth);
}

TEST_F(HanKerningTest, ResetFeatures) {
  Font* noto_cjk = CreateNotoCjk();
  const FontDescription& font_description = noto_cjk->GetFontDescription();
  const SimpleFontData* noto_cjk_data = noto_cjk->PrimaryFont();
  EXPECT_TRUE(noto_cjk_data);
  FontFeatureRanges features;
  features.push_back(FontFeatureRange{
      {{'T', 'E', 'S', 'T'}, 1}, 0, static_cast<unsigned>(-1)});
  EXPECT_EQ(features.size(), 1u);
  const String text(u"国）（国");
  {
    FontFeatureRangesSaver features_saver(&features);
    HanKerning han_kerning(text, 0, text.length(), font_description);
    han_kerning.AppendFontFeatures(text, 0, text.length(), *noto_cjk_data,
                                   font_description.LocaleOrDefault(),
                                   HanKerning::Options(), features);
    EXPECT_EQ(features.size(), 2u);
  }
  EXPECT_EQ(features.size(), 1u);
}

}  // namespace blink
