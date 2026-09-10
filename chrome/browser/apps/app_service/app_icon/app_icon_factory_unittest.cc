// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace apps {
class AppIconFactoryTest : public testing::Test {
 public:
  base::FilePath GetPath() {
    return tmp_dir_.GetPath().Append(
        base::FilePath::FromUTF8Unsafe("icon.file"));
  }

  void SetUp() override { ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir()); }

  bool RunLoadIconFromFileWithFallback(apps::IconValuePtr fallback_response,
                                       apps::IconValuePtr* result) {
    bool fallback_called = false;

    base::test::TestFuture<apps::IconValuePtr> success_future;
    apps::LoadIconFromFileWithFallback(
        apps::IconType::kUncompressed, 200, GetPath(), apps::IconEffects::kNone,
        success_future.GetCallback(),
        base::BindLambdaForTesting([&](apps::LoadIconCallback callback) {
          fallback_called = true;
          std::move(callback).Run(std::move(fallback_response));
        }));

    *result = success_future.Take();
    return fallback_called;
  }

  std::string GetPngData(const std::string& file_name) {
    base::FilePath base_path;
    std::string png_data_as_string;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_path));
    base::FilePath icon_file_path = base_path.AppendASCII("chromeos")
                                        .AppendASCII("ash")
                                        .AppendASCII("experiences")
                                        .AppendASCII("arc")
                                        .AppendASCII("test")
                                        .AppendASCII("data")
                                        .AppendASCII("icons")
                                        .AppendASCII(file_name);
    CHECK(base::PathExists(icon_file_path));
    CHECK(base::ReadFileToString(icon_file_path, &png_data_as_string));
    return png_data_as_string;
  }

  void RunLoadIconFromCompressedData(const std::string& png_data_as_string,
                                     apps::IconType icon_type,
                                     apps::IconEffects icon_effects,
                                     apps::IconValuePtr& output_icon) {
    base::test::TestFuture<apps::IconValuePtr> future;
    apps::LoadIconFromCompressedData(icon_type, kSizeInDip, icon_effects,
                                     png_data_as_string, future.GetCallback());
    output_icon = future.Take();
    ASSERT_TRUE(output_icon);
    ASSERT_EQ(icon_type, output_icon->icon_type);
    ASSERT_FALSE(output_icon->is_placeholder_icon);
    ASSERT_FALSE(output_icon->uncompressed.isNull());

    EnsureRepresentationsLoaded(output_icon->uncompressed);
  }

  void GenerateIconFromCompressedData(const std::string& compressed_icon,
                                      float scale,
                                      gfx::ImageSkia& output_image_skia) {
    SkBitmap decoded =
        gfx::PNGCodec::Decode(base::as_byte_span(compressed_icon));
    ASSERT_FALSE(decoded.isNull());

    output_image_skia = apps::CreateStandardIconImage(
        gfx::ImageSkia::CreateFromBitmap(decoded, scale));
    EnsureRepresentationsLoaded(output_image_skia);
  }

 protected:
  content::BrowserTaskEnvironment task_env_;
  base::ScopedTempDir tmp_dir_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(AppIconFactoryTest, LoadFromFileSuccess) {
  gfx::ImageSkia image =
      gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(20, 20), 0.0f));
  const SkBitmap* bitmap = image.bitmap();
  ASSERT_TRUE(
      cc::WritePNGFile(*bitmap, GetPath(), /*discard_transparency=*/false));

  auto fallback_response = std::make_unique<apps::IconValue>();
  auto result = std::make_unique<apps::IconValue>();
  bool fallback_called =
      RunLoadIconFromFileWithFallback(std::move(fallback_response), &result);
  EXPECT_FALSE(fallback_called);
  ASSERT_TRUE(result);

  EXPECT_TRUE(cc::MatchesBitmap(*bitmap, *result->uncompressed.bitmap(),
                                cc::ExactPixelComparator()));
}

TEST_F(AppIconFactoryTest, LoadFromFileFallback) {
  auto expect_image =
      gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(20, 20), 0.0f));

  auto fallback_response = std::make_unique<apps::IconValue>();
  fallback_response->icon_type = apps::IconType::kUncompressed;
  // Create a non-null image so we can check if we get the same image back.
  fallback_response->uncompressed = expect_image;

  auto result = std::make_unique<apps::IconValue>();
  bool fallback_called =
      RunLoadIconFromFileWithFallback(std::move(fallback_response), &result);
  EXPECT_TRUE(fallback_called);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->uncompressed.BackedBySameObjectAs(expect_image));
}

TEST_F(AppIconFactoryTest, LoadFromFileFallbackFailure) {
  auto fallback_response = std::make_unique<apps::IconValue>();
  auto result = std::make_unique<apps::IconValue>();
  bool fallback_called =
      RunLoadIconFromFileWithFallback(std::move(fallback_response), &result);
  EXPECT_TRUE(fallback_called);
  ASSERT_TRUE(result);
}

TEST_F(AppIconFactoryTest, LoadFromFileFallbackDoesNotReturn) {
  base::test::TestFuture<apps::IconValuePtr> success_future;

  bool fallback_called = false;
  apps::LoadIconFromFileWithFallback(
      apps::IconType::kUncompressed, /*size_hint_in_dip=*/200, GetPath(),
      apps::IconEffects::kNone, success_future.GetCallback(),
      base::BindLambdaForTesting([&](apps::LoadIconCallback) {
        // Drop the callback here, like a buggy fallback might.
        fallback_called = true;
      }));

  EXPECT_TRUE(success_future.Wait());
  EXPECT_TRUE(fallback_called);
  auto result = success_future.Take();
  ASSERT_TRUE(result);
}

TEST_F(AppIconFactoryTest, LoadIconFromCompressedData) {
  std::string png_data_as_string = GetPngData("icon_100p.png");

  auto icon_type = apps::IconType::kStandard;
  auto icon_effects = apps::IconEffects::kCrOsStandardIcon;

  auto result = std::make_unique<apps::IconValue>();
  RunLoadIconFromCompressedData(png_data_as_string, icon_type, icon_effects,
                                result);

  float scale = 1.0;
  gfx::ImageSkia src_image_skia;
  GenerateIconFromCompressedData(png_data_as_string, scale, src_image_skia);

  ASSERT_FALSE(src_image_skia.isNull());
  ASSERT_TRUE(src_image_skia.HasRepresentation(scale));
  ASSERT_TRUE(result->uncompressed.HasRepresentation(scale));
  ASSERT_TRUE(gfx::test::AreBitmapsEqual(
      src_image_skia.GetRepresentation(scale).GetBitmap(),
      result->uncompressed.GetRepresentation(scale).GetBitmap()));
}

}  // namespace apps
