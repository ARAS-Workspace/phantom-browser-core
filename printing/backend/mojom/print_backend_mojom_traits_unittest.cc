// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "printing/backend/mojom/print_backend.mojom.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

TEST(PrintBackendMojomTraitsTest, PrinterBasicInfo) {
  static const PrinterBasicInfo kPrinterBasicInfo1(
      /*printer_name=*/"test printer name 1",
      /*display_name=*/"test display name 1",
      /*printer_description=*/"This is printer #1 for unit testing.",
      /*options=*/{{"opt1", "123"}, {"opt2", "456"}});
  static const PrinterBasicInfo kPrinterBasicInfo2(
      /*printer_name=*/"test printer name 2",
      /*display_name=*/"test display name 2",
      /*printer_description=*/"This is printer #2 for unit testing.",
      /*options=*/{});
  static const PrinterBasicInfo kPrinterBasicInfo3(
      /*printer_name=*/"test printer name 2",
      /*display_name=*/"test display name 2",
      /*printer_description=*/"",
      /*options=*/{});
  static const PrinterList kPrinterList{kPrinterBasicInfo1, kPrinterBasicInfo2,
                                        kPrinterBasicInfo3};

  for (const auto& info : kPrinterList) {
    PrinterBasicInfo input = info;
    PrinterBasicInfo output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PrinterBasicInfo>(
        input, output));
    EXPECT_EQ(info, output);
  }
}

TEST(PrintBackendMojomTraitsTest, PrinterBasicInfoEmptyNames) {
  static const PrinterBasicInfo kPrinterBasicInfoEmptyPrinterName(
      /*printer_name=*/"",
      /*display_name=*/"test display name",
      /*printer_description=*/"",
      /*options=*/{});
  static const PrinterBasicInfo kPrinterBasicInfoEmptyDisplayName(
      /*printer_name=*/"test printer name",
      /*display_name=*/"",
      /*printer_description=*/"",
      /*options=*/{});
  static const PrinterList kPrinterList{kPrinterBasicInfoEmptyPrinterName,
                                        kPrinterBasicInfoEmptyDisplayName};

  for (const auto& info : kPrinterList) {
    PrinterBasicInfo input = info;
    PrinterBasicInfo output;
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::PrinterBasicInfo>(
        input, output));
  }
}

TEST(PrintBackendMojomTraitsTest, Paper) {
  PrinterSemanticCapsAndDefaults::Papers test_papers = kPapers;
  test_papers.push_back(kPaperCustom);

  for (const auto& paper : test_papers) {
    PrinterSemanticCapsAndDefaults::Paper input = paper;
    PrinterSemanticCapsAndDefaults::Paper output;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
    EXPECT_EQ(paper, output);
  }
}

TEST(PrintBackendMojomTraitsTest, PaperCtors) {
  // All constructors should be able to generate valid papers.
  constexpr gfx::Size kNonEmptySize(100, 200);
  constexpr gfx::Rect kNonEmptyPrintableArea(kNonEmptySize);
  PrinterSemanticCapsAndDefaults::Paper output;

  PrinterSemanticCapsAndDefaults::Paper input;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));

  input = PrinterSemanticCapsAndDefaults::Paper("display_name", "vendor_id",
                                                kNonEmptySize);
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));

  input = PrinterSemanticCapsAndDefaults::Paper(
      "display_name", "vendor_id", kNonEmptySize, kNonEmptyPrintableArea);
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));

  input = PrinterSemanticCapsAndDefaults::Paper(
      "display_name", "vendor_id", kNonEmptySize, kNonEmptyPrintableArea,
      /*max_height_um=*/200);
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));

  input = PrinterSemanticCapsAndDefaults::Paper(
      "display_name", "vendor_id", kNonEmptySize, kNonEmptyPrintableArea,
      /*max_height_um=*/200, /*has_borderless_variant=*/true);
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));

}

TEST(PrintBackendMojomTraitsTest, PaperEmpty) {
  // Empty Papers should be valid.
  PrinterSemanticCapsAndDefaults::Paper input;
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
  EXPECT_EQ(input, output);
}

TEST(PrintBackendMojomTraitsTest, PaperInvalidCustomSize) {
  // The min height is larger than the max height, so it should be invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name",
      /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(0, 0, 4000, 7000),
      /*max_height_um=*/6000,
      /*has_borderless_variant=*/true};
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, PaperEmptyPrintableArea) {
  // The printable area is empty, but the other fields are not, so it should be
  // invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name", /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(0, 100, 0, 0)};
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, PaperPrintableAreaLargerThanSize) {
  // The printable area is larger than the size, so it should be invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name", /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(0, 100, 4100, 7200)};
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, PaperPrintableAreaLargerThanCustomSize) {
  // The printable area is larger than the custom size, so it should be invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name",
      /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(0, 100, 4100, 7200),
      /*max_height_um=*/8000};
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, PaperPrintableAreaOutOfBounds) {
  // The printable area is out of bounds of the size, so it should be invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name", /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(4050, 6950, 100, 100)};
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, PaperNegativePrintableArea) {
  // The printable area has negative x and y values, so it should be invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name", /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(-10, -10, 2800, 6000)};
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, PrinterSemanticCapsAndDefaults) {
  OptionalSampleCapabilities caps;
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults(std::move(caps));
  PrinterSemanticCapsAndDefaults output;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kCollateCapable, output.collate_capable);
  EXPECT_EQ(kCollateDefault, output.collate_default);
  EXPECT_EQ(kCopiesMax, output.copies_max);
  EXPECT_EQ(kDuplexModes, output.duplex_modes);
  EXPECT_EQ(kDuplexDefault, output.duplex_default);
  EXPECT_EQ(kColorChangeable, output.color_changeable);
  EXPECT_EQ(kColorDefault, output.color_default);
  EXPECT_EQ(kColorModel, output.color_model);
  EXPECT_EQ(kBwModel, output.bw_model);
  EXPECT_EQ(kPapers, output.papers);
  EXPECT_EQ(kUserDefinedPapers, output.user_defined_papers);
  EXPECT_TRUE(kDefaultPaper == output.default_paper);
  EXPECT_EQ(kDpis, output.dpis);
  EXPECT_EQ(kDefaultDpi, output.default_dpi);
  EXPECT_EQ(kMediaTypes, output.media_types);
  EXPECT_EQ(kDefaultMediaType, output.default_media_type);
}

TEST(PrintBackendMojomTraitsTest, PrinterSemanticCapsAndDefaultsCopiesMax) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults({});
  PrinterSemanticCapsAndDefaults output;

  // Override sample with no copies.
  input.copies_max = 0;

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::PrinterSemanticCapsAndDefaults>(input, output));
}

TEST(PrintBackendMojomTraitsTest,
     PrinterSemanticCapsAndDefaultsAllowableEmptyArrays) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults({});
  PrinterSemanticCapsAndDefaults output;

  // Override sample with arrays which are allowed to be empty:
  // `duplex_modes`, `user_defined_papers`, `dpis`, `advanced_capabilities`.
  const std::vector<mojom::DuplexMode> kEmptyDuplexModes;
  const PrinterSemanticCapsAndDefaults::Papers kEmptyUserDefinedPapers;
  const std::vector<gfx::Size> kEmptyDpis;

  input.duplex_modes = kEmptyDuplexModes;
  input.user_defined_papers = kEmptyUserDefinedPapers;
  input.dpis = kEmptyDpis;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kEmptyDuplexModes, output.duplex_modes);
  EXPECT_EQ(kEmptyUserDefinedPapers, output.user_defined_papers);
  EXPECT_EQ(kEmptyDpis, output.dpis);
}

TEST(PrintBackendMojomTraitsTest, PrinterSemanticCapsAndDefaultsEmptyPapers) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults({});
  PrinterSemanticCapsAndDefaults output;

  // Override sample with empty `papers`.  This is known to be possible, seen
  // with Epson PX660 series driver.
  const PrinterSemanticCapsAndDefaults::Papers kEmptyPapers;
  input.papers.clear();

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kEmptyPapers, output.papers);
}

TEST(PrintBackendMojomTraitsTest,
     PrinterSemanticCapsAndDefaultsEmptyMediaTypes) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults({});
  PrinterSemanticCapsAndDefaults output;

  // Override sample with empty `media_types`.
  const PrinterSemanticCapsAndDefaults::MediaTypes kEmptyMediaTypes;
  input.media_types.clear();

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kEmptyMediaTypes, output.media_types);
}

TEST(PrintBackendMojomTraitsTest,
     PrinterSemanticCapsAndDefaultsNoDuplicatesInArrays) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults({});
  PrinterSemanticCapsAndDefaults output;

  // Override sample with arrays containing duplicates, which is not allowed.
  input.duplex_modes = {mojom::DuplexMode::kLongEdge,
                        mojom::DuplexMode::kSimplex,
                        mojom::DuplexMode::kSimplex};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::PrinterSemanticCapsAndDefaults>(input, output));

  input = GenerateSamplePrinterSemanticCapsAndDefaults({});
  input.user_defined_papers = {kPaperLetter, kPaperLetter};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::PrinterSemanticCapsAndDefaults>(input, output));

}

TEST(PrintBackendMojomTraitsTest,
     PrinterSemanticCapsAndDefaultsAllowedDuplicatesInArrays) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults({});
  PrinterSemanticCapsAndDefaults output;

  // Override sample with arrays containing duplicates where it is allowed.
  // Duplicate DPIs are known to be possible, seen with the Kyocera KX driver.
  const std::vector<gfx::Size> kDuplicateDpis{kDpi600, kDpi600, kDpi1200};
  input.dpis = kDuplicateDpis;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kDuplicateDpis, output.dpis);

  // Duplicate papers are known to be possible, seen with the Konica Minolta
  // 4750 Series PS driver.
  // Use a paper with same name but different size.
  PrinterSemanticCapsAndDefaults::Paper paper_a4_prime(
      kPaperA4.display_name(), kPaperA4.vendor_id(), kPaperLetter.size_um(),
      kPaperA4.printable_area_um());
  input = GenerateSamplePrinterSemanticCapsAndDefaults({});
  const PrinterSemanticCapsAndDefaults::Papers kDuplicatePapers{
      kPaperA4, kPaperLetter, kPaperLedger, paper_a4_prime};
  input.papers = kDuplicatePapers;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kDuplicatePapers, output.papers);
}

}  // namespace printing
