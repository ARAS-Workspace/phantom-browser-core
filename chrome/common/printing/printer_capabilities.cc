// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/printing/printer_capabilities.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/printing/printing_buildflags.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/device_event_log/device_event_log.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "components/strings/grit/components_strings.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
#include "components/printing/common/print_media_l10n.h"
#endif  // BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)

namespace printing {

const char kPrinter[] = "printer";

namespace {

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
// Iterate on the `Papers` of a given printer `info` and set the
// `display_name` members, localizing where possible, as well as the `vendor_id`
// members. The `Papers` will be sorted in place when this function returns.
void PopulateAndSortAllPaperNames(PrinterSemanticCapsAndDefaults& info) {
  MediaSizeInfo default_paper =
      LocalizePaperDisplayName(info.default_paper.size_um());
  info.default_paper.set_display_name(
      base::UTF16ToUTF8(default_paper.display_name));
  info.default_paper.set_vendor_id(default_paper.vendor_id);

  // Pair the paper entries with their sort info so they can be sorted.
  std::vector<PaperWithSizeInfo> size_list;
  for (PrinterSemanticCapsAndDefaults::Paper& paper : info.papers) {
    // Copy `paper.size_um` to avoid potentially using `paper` after calling
    // std::move(paper).
    gfx::Size size_um = paper.size_um();
    size_list.emplace_back(LocalizePaperDisplayName(size_um), std::move(paper));
  }

  // Sort and recreate the list with localizations inserted.
  SortPaperDisplayNames(size_list);
  info.papers.clear();
  for (auto& pair : size_list) {
    auto& paper = info.papers.emplace_back(std::move(pair.paper));
    paper.set_display_name(base::UTF16ToUTF8(pair.size_info.display_name));
    paper.set_vendor_id(pair.size_info.vendor_id);
  }
}
#endif  // BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)

// Returns a dictionary representing printer capabilities as CDD, or
// a Value of type NONE if no capabilities are provided.
base::Value AssemblePrinterCapabilities(const std::string& device_name,
                                        bool has_secure_protocol,
                                        PrinterSemanticCapsAndDefaults* caps) {
  DCHECK(!device_name.empty());
  if (!caps)
    return base::Value();

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
  // TODO(crbug.com/339188518): Is this needed on Linux? If so, need to add back
  // the `features::kCupsIppPrintingBackend` check when the
  // `enable_print_media_l10n` GN variable gets set to true for Linux.
  PopulateAndSortAllPaperNames(*caps);
#endif  // BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)

  return cloud_print::PrinterSemanticCapsAndDefaultsToCdd(*caps);
}

}  // namespace

base::DictValue AssemblePrinterSettings(const std::string& device_name,
                                        const PrinterBasicInfo& basic_info,
                                        bool has_secure_protocol,
                                        PrinterSemanticCapsAndDefaults* caps) {
  base::DictValue printer_info;
  printer_info.Set(kSettingDeviceName, device_name);
  printer_info.Set(kSettingPrinterName, basic_info.display_name);
  printer_info.Set(kSettingPrinterDescription, basic_info.printer_description);

  base::DictValue options;

  printer_info.Set(kSettingPrinterOptions, std::move(options));

  base::DictValue printer_info_capabilities;
  printer_info_capabilities.Set(kPrinter, std::move(printer_info));
  base::Value capabilities =
      AssemblePrinterCapabilities(device_name, has_secure_protocol, caps);
  if (capabilities.is_dict()) {
    printer_info_capabilities.Set(kSettingCapabilities,
                                  std::move(capabilities));
  }
  return printer_info_capabilities;
}

base::DictValue GetSettingsOnBlockingTaskRunner(
    const std::string& device_name,
    const PrinterBasicInfo& basic_info,
    PrinterSemanticCapsAndDefaults::Papers user_defined_papers,
    scoped_refptr<PrintBackend> print_backend) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  PRINTER_LOG(EVENT) << "Get printer capabilities start for " << device_name;
  const std::vector<std::string> driver_info =
      print_backend->GetPrinterDriverInfo(device_name);
  PRINTER_LOG(EVENT) << "Driver info: " << base::JoinString(driver_info, ";");

  crash_keys::ScopedPrinterInfo crash_key(device_name, driver_info);

  auto caps = std::make_optional<PrinterSemanticCapsAndDefaults>();
  mojom::ResultCode result =
      print_backend->GetPrinterSemanticCapsAndDefaults(device_name, &*caps);
  if (result == mojom::ResultCode::kSuccess) {
    PRINTER_LOG(EVENT) << "Got printer capabilities for " << device_name;
    caps->user_defined_papers = std::move(user_defined_papers);
  } else {
    // Failed to get capabilities, but proceed to assemble the settings to
    // return what information we do have.
    PRINTER_LOG(ERROR) << "Failed to get capabilities for " << device_name
                       << ", result: " << result;
    caps = std::nullopt;
  }

  return AssemblePrinterSettings(device_name, basic_info,
                                 /*has_secure_protocol=*/false,
                                 base::OptionalToPtr(caps));
}

}  // namespace printing
