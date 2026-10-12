// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Error codes returned in response to $(ref:onPrintRequested) event.
enum PrintError {
  // Specifies that the operation was completed successfully.
  "OK",

  // Specifies that a general failure occured.
  "FAILED",

  // Specifies that the print ticket is invalid. For example, the ticket is
  // inconsistent with some capabilities, or the extension is not able to
  // handle all settings from the ticket.
  "INVALID_TICKET",

  // Specifies that the document is invalid. For example, data may be
  // corrupted or the format is incompatible with the extension.
  "INVALID_DATA"
};

// Printer description for $(ref:onGetPrintersRequested) event.
dictionary PrinterInfo {
  // Unique printer ID.
  required DOMString id;

  // Printer's human readable name.
  required DOMString name;

  // Printer's human readable description.
  DOMString description;
};

// Printing request parameters. Passed to $(ref:onPrintRequested) event.
dictionary PrintJob {
  // ID of the printer which should handle the job.
  required DOMString printerId;

  // The print job title.
  required DOMString title;

  // Print ticket in
  // <a href="https://developers.google.com/cloud-print/docs/cdd#cjt">
  // CJT format</a>.
  // <aside class="aside flow bg-state-info-bg color-state-info-text">
  // <div class="flow">The CJT reference is marked as deprecated. It is
  // deprecated for Google Cloud Print only. is not deprecated for
  // ChromeOS printing.
  // </div>
  // </aside>
  required object ticket;

  // The document content type. Supported formats are
  // <code>"application/pdf"</code> and <code>"image/pwg-raster"</code>.
  required DOMString contentType;

  // Blob containing the document data to print. Format must match
  // |contentType|.
  [instanceOf=Blob] required object document;
};

callback PrintersCallback = undefined (sequence<PrinterInfo> printerInfo);

// |capabilities|: Device capabilities in
// <a href="https://developers.google.com/cloud-print/docs/cdd#cdd">CDD
// format</a>.
callback CapabilitiesCallback = undefined (object capabilities);

callback PrintCallback = undefined (PrintError result);

// Listener callback for the onGetPrintersRequested event.
// |resultCallback|: Callback to return printer list. Every listener must
// call callback exactly once.
callback OnGetPrintersRequestedListener = undefined (PrintersCallback resultCallback);

interface OnGetPrintersRequestedEvent : ExtensionEvent {
  static undefined addListener(OnGetPrintersRequestedListener listener);
  static undefined removeListener(OnGetPrintersRequestedListener listener);
  static boolean hasListener(OnGetPrintersRequestedListener listener);
};

// Listener callback for the onGetCapabilityRequested event.
// |printerId|: Unique ID of the printer whose capabilities are requested.
// |resultCallback|: Callback to return device capabilities in
// <a href="https://developers.google.com/cloud-print/docs/cdd#cdd">CDD
// format</a>.
// The receiving listener must call callback exectly once.
callback OnGetCapabilityRequestedListener = undefined (
    DOMString printerId,
    CapabilitiesCallback resultCallback);

interface OnGetCapabilityRequestedEvent : ExtensionEvent {
  static undefined addListener(OnGetCapabilityRequestedListener listener);
  static undefined removeListener(OnGetCapabilityRequestedListener listener);
  static boolean hasListener(OnGetCapabilityRequestedListener listener);
};

// Listener callback for the onPrintRequested event.
// |printJob|: The printing request parameters.
// |resultCallback|: Callback that should be called when the printing
// request is completed.
callback OnPrintRequestedListener = undefined (
    PrintJob printJob,
    PrintCallback resultCallback);

interface OnPrintRequestedEvent : ExtensionEvent {
  static undefined addListener(OnPrintRequestedListener listener);
  static undefined removeListener(OnPrintRequestedListener listener);
  static boolean hasListener(OnPrintRequestedListener listener);
};

// The <code>chrome.printerProvider</code> API exposes events used by print
// manager to query printers controlled by extensions, to query their
// capabilities and to submit print jobs to these printers.
interface PrinterProvider {
  // Event fired when print manager requests printers provided by extensions.
  static attribute OnGetPrintersRequestedEvent onGetPrintersRequested;

  // Event fired when print manager requests printer capabilities.
  static attribute OnGetCapabilityRequestedEvent onGetCapabilityRequested;

  // Event fired when print manager requests printing.
  static attribute OnPrintRequestedEvent onPrintRequested;
};

partial interface Browser {
  static attribute PrinterProvider printerProvider;
};
