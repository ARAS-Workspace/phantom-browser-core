// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font_data/font_data_service_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/services/font_data/public/mojom/font_data_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace font_data_service {

namespace {

class TestFontDataService : public FontDataServiceImpl {
 public:
  TestFontDataService() = default;
  ~TestFontDataService() override = default;
  TestFontDataService(const TestFontDataService&) = delete;
  TestFontDataService& operator=(const TestFontDataService&) = delete;

  std::tuple<base::File, uint64_t> GetFileHandle(
      SkTypeface& typeface) override {
    if (use_memory_fallback_) {
      // Return an empty file handle to simulate the fallback.
      return {base::File(), 0UL};
    }
    return FontDataServiceImpl::GetFileHandle(typeface);
  }

  void set_use_memory_fallback(bool fallback) {
    use_memory_fallback_ = fallback;
  }

  bool CheckMatchesRequiredStyleForTesting(
      const SkFontStyle& actual_style,
      const std::string& requested_family_name,
      const SkFontStyle& requested_style) {
    return CheckMatchesRequiredStyle(actual_style, requested_family_name,
                                     requested_style);
  }

 private:
  bool use_memory_fallback_ = false;
};

class FontDataServiceImplUnitTest : public testing::Test {
 protected:
  FontDataServiceImplUnitTest()
      : receiver_(&impl_, font_service_.BindNewPipeAndPassReceiver()) {}
  ~FontDataServiceImplUnitTest() override = default;

  base::test::SingleThreadTaskEnvironment environment_;
  mojo::Remote<mojom::FontDataService> font_service_;
  TestFontDataService impl_;
  mojo::Receiver<mojom::FontDataService> receiver_;
};

mojom::TypefaceStylePtr CreateTypefaceStyle(int weight,
                                            int width,
                                            mojom::TypefaceSlant slant) {
  mojom::TypefaceStylePtr style(mojom::TypefaceStyle::New());
  style->weight = weight;
  style->width = width;
  style->slant = slant;
  return style;
}

TEST_F(FontDataServiceImplUnitTest, MatchFamilyName) {
  mojom::MatchFamilyNameResultPtr out_result;
  std::string family_name = "Arimo";
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 0u);

  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
  // During the Skia transition, Linux/ChromeOS may return either a font file
  // (if getResourceName() is supported) or hit the memory region fallback.
  // TODO(crbug.com/463411679): Remove the memory fallback check once Skia
  // change has rolled into Chromium.
  if (out_result->typeface_data->is_font_file()) {
    EXPECT_EQ(impl_.GetCacheSizeForTesting(), 0u);
    EXPECT_TRUE(
        out_result->typeface_data->get_font_file()->file_handle.IsValid());
  } else {
    EXPECT_EQ(impl_.GetCacheSizeForTesting(), 1u);
    EXPECT_TRUE(out_result->typeface_data->is_region());
    EXPECT_TRUE(out_result->typeface_data->get_region().IsValid());
  }
}

TEST_F(FontDataServiceImplUnitTest, MatchFamilyNameMemoryCacheSize) {
  mojom::MatchFamilyNameResultPtr out_result;
  std::string family_name = "Arimo";
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 0u);
  impl_.set_use_memory_fallback(true);

  // There should be one entry added to the cache.
  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
  EXPECT_TRUE(out_result->typeface_data->is_region());
  EXPECT_TRUE(out_result->typeface_data->get_region().IsValid());
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 1u);

  // Call with the same family name and style. Cache should stay the same
  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 1u);

  // Call with a different family name. Cache should increase.
  family_name = "Tinos";
  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 2u);

  // Call with a different font style. Cache should increase.
  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(600, 5, mojom::TypefaceSlant::kOblique),
      &out_result);
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 3u);

  // Call with a gibberish family name. Cache should be the same. Result should
  // be nullptr.
  font_service_->MatchFamilyName(
      "not a real font",
      CreateTypefaceStyle(600, 5, mojom::TypefaceSlant::kOblique), &out_result);
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 3u);
  EXPECT_EQ(out_result.get(), nullptr);
}

// The Linux/ChromeOS SkFontMgr doesn't support MatchFamilyStyleCharacter().

// The Linux/ChromeOS SkFontMgr doesn't support MatchFamilyStyleCharacter().

// The Linux/ChromeOS SkFontMgr doesn't support countFamilies().

TEST_F(FontDataServiceImplUnitTest, LegacyMakeTypefaceNullFamilyName) {
  mojom::MatchFamilyNameResultPtr out_result;

  // LegacyMakeTypeface should return the default font if `family_name` is null.
  font_service_->LegacyMakeTypeface(
      std::nullopt, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
  // During the Skia transition, Linux/ChromeOS may return either a font file
  // or hit the memory region fallback.
  if (out_result->typeface_data->is_font_file()) {
    EXPECT_TRUE(
        out_result->typeface_data->get_font_file()->file_handle.IsValid());
  } else {
    EXPECT_TRUE(out_result->typeface_data->is_region());
    EXPECT_TRUE(out_result->typeface_data->get_region().IsValid());
  }
}

}  // namespace

}  // namespace font_data_service
