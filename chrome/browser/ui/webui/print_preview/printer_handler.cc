// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/printer_handler.h"

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"
#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"
#include "chrome/common/buildflags.h"

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"

namespace printing {

// static
std::unique_ptr<PrinterHandler> PrinterHandler::CreateForExtensionPrinters(
    Profile* profile) {
  return std::make_unique<ExtensionPrinterHandler>(profile);
}

// static
std::unique_ptr<PrinterHandler> PrinterHandler::CreateForLocalPrinters(
    content::WebContents* preview_web_contents,
    Profile* profile) {
  return std::make_unique<LocalPrinterHandlerDefault>(preview_web_contents);
}

// static
std::unique_ptr<PrinterHandler> PrinterHandler::CreateForPdfPrinter(
    Profile* profile,
    content::WebContents* preview_web_contents,
    PrintPreviewStickySettings* sticky_settings) {
  return std::make_unique<PdfPrinterHandler>(profile, preview_web_contents,
                                             sticky_settings);
}

void PrinterHandler::GetDefaultPrinter(DefaultPrinterCallback cb) {
  NOTREACHED();
}

void PrinterHandler::StartGrantPrinterAccess(const std::string& printer_id,
                                             GetPrinterInfoCallback callback) {
  NOTREACHED();
}

}  // namespace printing
