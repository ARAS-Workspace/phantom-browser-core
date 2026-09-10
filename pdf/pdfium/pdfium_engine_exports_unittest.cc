// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/test/mock_callback.h"
#include "build/chromeos_buildflags.h"
#include "pdf/pdf.h"
#include "pdf/pdfium/pdfium_api_wrappers.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_text.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace chrome_pdf {

namespace {

class PDFiumEngineExportsTest : public testing::Test {
 public:
  PDFiumEngineExportsTest() = default;
  PDFiumEngineExportsTest(const PDFiumEngineExportsTest&) = delete;
  PDFiumEngineExportsTest& operator=(const PDFiumEngineExportsTest&) = delete;
  ~PDFiumEngineExportsTest() override = default;

 protected:
  void SetUp() override {
    pdf_data_dir_ = base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
                        .Append(FILE_PATH_LITERAL("pdf"))
                        .Append(FILE_PATH_LITERAL("test"))
                        .Append(FILE_PATH_LITERAL("data"));
  }

  const base::FilePath& pdf_data_dir() const { return pdf_data_dir_; }

 private:
  base::FilePath pdf_data_dir_;
};

}  // namespace

TEST_F(PDFiumEngineExportsTest, GetPDFDocInfo) {
  base::FilePath pdf_path =
      pdf_data_dir().Append(FILE_PATH_LITERAL("hello_world2.pdf"));
  std::optional<std::vector<uint8_t>> pdf_data =
      base::ReadFileToBytes(pdf_path);
  ASSERT_TRUE(pdf_data.has_value());

  ASSERT_TRUE(GetPDFDocInfo(pdf_data.value(), nullptr, nullptr));

  int page_count;
  float max_page_width;
  ASSERT_TRUE(GetPDFDocInfo(pdf_data.value(), &page_count, &max_page_width));
  EXPECT_EQ(2, page_count);
  EXPECT_DOUBLE_EQ(200.0, max_page_width);
}

TEST_F(PDFiumEngineExportsTest, GetPDFPageSizeByIndex) {
  // TODO(thestig): Use a better PDF for this test, as hello_world2.pdf's page
  // dimensions are uninteresting.
  base::FilePath pdf_path =
      pdf_data_dir().Append(FILE_PATH_LITERAL("hello_world2.pdf"));
  std::optional<std::vector<uint8_t>> pdf_data =
      base::ReadFileToBytes(pdf_path);
  ASSERT_TRUE(pdf_data.has_value());

  int page_count;
  ASSERT_TRUE(GetPDFDocInfo(pdf_data.value(), &page_count, nullptr));
  ASSERT_EQ(2, page_count);
  for (int page_index = 0; page_index < page_count; ++page_index) {
    std::optional<gfx::SizeF> page_size =
        GetPDFPageSizeByIndex(pdf_data.value(), page_index);
    ASSERT_TRUE(page_size.has_value());
    EXPECT_EQ(gfx::SizeF(200, 200), page_size.value());
  }
}

TEST_F(PDFiumEngineExportsTest, ConvertPdfPagesToNupPdf) {
  base::FilePath pdf_path =
      pdf_data_dir().Append(FILE_PATH_LITERAL("rectangles.pdf"));
  std::optional<std::vector<uint8_t>> pdf_data =
      base::ReadFileToBytes(pdf_path);
  ASSERT_TRUE(pdf_data.has_value());

  std::vector<base::span<const uint8_t>> pdf_buffers;
  std::vector<uint8_t> output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 1, gfx::Size(612, 792), gfx::Rect(22, 20, 570, 750));
  EXPECT_TRUE(output_pdf_buffer.empty());

  pdf_buffers.push_back(pdf_data.value());
  pdf_buffers.push_back(pdf_data.value());
  output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 2, gfx::Size(612, 792), gfx::Rect(22, 20, 0, 750));
  EXPECT_TRUE(output_pdf_buffer.empty());
  output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 2, gfx::Size(612, 792), gfx::Rect(22, 20, 570, 0));
  EXPECT_TRUE(output_pdf_buffer.empty());
  output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 2, gfx::Size(612, 792), gfx::Rect(300, 20, 570, 750));
  EXPECT_TRUE(output_pdf_buffer.empty());
  output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 2, gfx::Size(612, 792), gfx::Rect(22, 400, 570, 750));
  EXPECT_TRUE(output_pdf_buffer.empty());
  output_pdf_buffer = ConvertPdfPagesToNupPdf(
      pdf_buffers, 2, gfx::Size(612, 792), gfx::Rect(22, 20, 570, 750));
  ASSERT_GT(output_pdf_buffer.size(), 0U);

  base::span<const uint8_t> output_pdf_span = base::span(output_pdf_buffer);
  int page_count;
  ASSERT_TRUE(GetPDFDocInfo(output_pdf_span, &page_count, nullptr));
  ASSERT_EQ(1, page_count);

  std::optional<gfx::SizeF> page_size =
      GetPDFPageSizeByIndex(output_pdf_span, 0);
  ASSERT_TRUE(page_size.has_value());
  EXPECT_EQ(gfx::SizeF(792, 612), page_size.value());
}

TEST_F(PDFiumEngineExportsTest, ConvertPdfDocumentToNupPdf) {
  base::FilePath pdf_path =
      pdf_data_dir().Append(FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  std::optional<std::vector<uint8_t>> pdf_data =
      base::ReadFileToBytes(pdf_path);
  ASSERT_TRUE(pdf_data.has_value());

  std::vector<uint8_t> output_pdf_buffer = ConvertPdfDocumentToNupPdf(
      {}, 1, gfx::Size(612, 792), gfx::Rect(32, 20, 570, 750));
  EXPECT_TRUE(output_pdf_buffer.empty());

  output_pdf_buffer = ConvertPdfDocumentToNupPdf(
      pdf_data.value(), 4, gfx::Size(612, 792), gfx::Rect(22, 20, 570, 750));
  ASSERT_GT(output_pdf_buffer.size(), 0U);

  base::span<const uint8_t> output_pdf_span = base::span(output_pdf_buffer);
  int page_count;
  ASSERT_TRUE(GetPDFDocInfo(output_pdf_span, &page_count, nullptr));
  ASSERT_EQ(2, page_count);
  for (int page_index = 0; page_index < page_count; ++page_index) {
    std::optional<gfx::SizeF> page_size =
        GetPDFPageSizeByIndex(output_pdf_span, page_index);
    ASSERT_TRUE(page_size.has_value());
    EXPECT_EQ(gfx::SizeF(612, 792), page_size.value());
  }
}

}  // namespace chrome_pdf
