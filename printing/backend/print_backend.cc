// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include <optional>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"

namespace {

// PrintBackend override for testing.
printing::PrintBackend* g_print_backend_for_test = nullptr;

}  // namespace

namespace printing {

PrinterBasicInfo::PrinterBasicInfo() = default;

PrinterBasicInfo::PrinterBasicInfo(const std::string& printer_name,
                                   const std::string& display_name,
                                   const std::string& printer_description,
                                   const PrinterBasicInfoOptions& options)
    : printer_name(printer_name),
      display_name(display_name),
      printer_description(printer_description),
      options(options) {}

PrinterBasicInfo::PrinterBasicInfo(const PrinterBasicInfo& other) = default;

PrinterBasicInfo::~PrinterBasicInfo() = default;

bool PrinterBasicInfo::operator==(const PrinterBasicInfo& other) const {
  return printer_name == other.printer_name &&
         display_name == other.display_name &&
         printer_description == other.printer_description &&
         options == other.options;
}

PrinterSemanticCapsAndDefaults::Paper::Paper() = default;

PrinterSemanticCapsAndDefaults::Paper::Paper(const std::string& display_name,
                                             const std::string& vendor_id,
                                             const gfx::Size& size_um)
    : Paper(display_name, vendor_id, size_um, gfx::Rect(size_um)) {}

PrinterSemanticCapsAndDefaults::Paper::Paper(const std::string& display_name,
                                             const std::string& vendor_id,
                                             const gfx::Size& size_um,
                                             const gfx::Rect& printable_area_um)
    : Paper(display_name,
            vendor_id,
            size_um,
            printable_area_um,
            /*max_height_um=*/0,
            /*has_borderless_variant=*/false) {}

PrinterSemanticCapsAndDefaults::Paper::Paper(const std::string& display_name,
                                             const std::string& vendor_id,
                                             const gfx::Size& size_um,
                                             const gfx::Rect& printable_area_um,
                                             int max_height_um)
    : Paper(display_name,
            vendor_id,
            size_um,
            printable_area_um,
            max_height_um,
            /*has_borderless_variant=*/false) {}

PrinterSemanticCapsAndDefaults::Paper::Paper(
    const std::string& display_name,
    const std::string& vendor_id,
    const gfx::Size& size_um,
    const gfx::Rect& printable_area_um,
    int max_height_um,
    bool has_borderless_variant
    )
    : display_name_(display_name),
      vendor_id_(vendor_id),
      size_um_(size_um),
      printable_area_um_(printable_area_um),
      max_height_um_(max_height_um),
      has_borderless_variant_(has_borderless_variant)
    {}

PrinterSemanticCapsAndDefaults::Paper::~Paper() = default;

PrinterSemanticCapsAndDefaults::Paper::Paper(const Paper& other) = default;

PrinterSemanticCapsAndDefaults::Paper&
PrinterSemanticCapsAndDefaults::Paper::operator=(const Paper& other) = default;

bool PrinterSemanticCapsAndDefaults::Paper::operator==(
    const PrinterSemanticCapsAndDefaults::Paper& other) const {
  return display_name_ == other.display_name_ &&
         vendor_id_ == other.vendor_id_ && size_um_ == other.size_um_ &&
         printable_area_um_ == other.printable_area_um_ &&
         max_height_um_ == other.max_height_um_ &&
         has_borderless_variant_ == other.has_borderless_variant_
      ;
}

bool PrinterSemanticCapsAndDefaults::Paper::SupportsCustomSize() const {
  return max_height_um_ > 0;
}

bool PrinterSemanticCapsAndDefaults::Paper::IsSizeWithinBounds(
    const gfx::Size& other_um) const {
  if (other_um == size_um_) {
    return true;
  }

  if (!SupportsCustomSize()) {
    return false;
  }

  return size_um_.width() == other_um.width() &&
         size_um_.height() <= other_um.height() &&
         other_um.height() <= max_height_um_;
}

bool PrinterSemanticCapsAndDefaults::MediaType::operator==(
    const PrinterSemanticCapsAndDefaults::MediaType& other) const {
  return display_name == other.display_name && vendor_id == other.vendor_id;
}

PrinterSemanticCapsAndDefaults::PrinterSemanticCapsAndDefaults() = default;

PrinterSemanticCapsAndDefaults::PrinterSemanticCapsAndDefaults(
    const PrinterSemanticCapsAndDefaults& other) = default;

PrinterSemanticCapsAndDefaults::~PrinterSemanticCapsAndDefaults() = default;

// This function is only supposed to be used in tests. The declaration in the
// header file is guarded by "#if defined(UNIT_TEST)" so that they can be used
// by tests but not non-test code. However, this .cc file is compiled as part of
// "backend" where "UNIT_TEST" is not defined. So we need to specify
// "COMPONENT_EXPORT(PRINT_BACKEND)" here again so that they are visible to
// tests.
COMPONENT_EXPORT(PRINT_BACKEND)
bool operator==(const PrinterSemanticCapsAndDefaults& caps1,
                const PrinterSemanticCapsAndDefaults& caps2) {
  return caps1.collate_capable == caps2.collate_capable &&
         caps1.collate_default == caps2.collate_default &&
         caps1.copies_max == caps2.copies_max &&
         caps1.duplex_modes == caps2.duplex_modes &&
         caps1.duplex_default == caps2.duplex_default &&
         caps1.color_changeable == caps2.color_changeable &&
         caps1.color_default == caps2.color_default &&
         caps1.color_model == caps2.color_model &&
         caps1.bw_model == caps2.bw_model && caps1.papers == caps2.papers &&
         caps1.user_defined_papers == caps2.user_defined_papers &&
         caps1.default_paper == caps2.default_paper &&
         caps1.dpis == caps2.dpis && caps1.default_dpi == caps2.default_dpi
      ;
}

PrinterCapsAndDefaults::PrinterCapsAndDefaults() = default;

PrinterCapsAndDefaults::PrinterCapsAndDefaults(
    const PrinterCapsAndDefaults& other) = default;

PrinterCapsAndDefaults::~PrinterCapsAndDefaults() = default;

PrintBackend::PrintBackend() = default;

PrintBackend::~PrintBackend() = default;

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstance(
    const std::string& locale) {
  return g_print_backend_for_test ? g_print_backend_for_test
                                  : PrintBackend::CreateInstanceImpl(locale);
}

// static
void PrintBackend::SetPrintBackendForTesting(PrintBackend* backend) {
  g_print_backend_for_test = backend;
}

}  // namespace printing
