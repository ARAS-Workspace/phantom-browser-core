// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="enable_pdf_ink2">
type GetTextInfoResult = chrome.pdfViewerPrivate.GetTextInfoResult;
// </if>

// TODO(crbug.com/40825351): Move the other chrome.pdfViewerPrivate calls across
// the PDF UI under this proxy.
// `chrome.pdfViewerPrivate.isAllowedLocalFileAccess` is currently located in
// `chrome/browser/resources/pdf/navigator.ts`.
export interface PdfViewerPrivateProxy {
  // <if expr="enable_pdf_ink2">
  getTextInfo(textarea: HTMLTextAreaElement, knownFontIds: number[]):
      Promise<GetTextInfoResult>;
  // </if>
  setPdfDocumentTitle(title: string): void;
}

export class PdfViewerPrivateProxyImpl implements PdfViewerPrivateProxy {
  // <if expr="enable_pdf_ink2">
  getTextInfo(textarea: HTMLTextAreaElement, knownFontIds: number[]):
      Promise<GetTextInfoResult> {
    return chrome.pdfViewerPrivate.getTextInfo(textarea, knownFontIds);
  }
  // </if>

  setPdfDocumentTitle(title: string): void {
    chrome.pdfViewerPrivate.setPdfDocumentTitle(title);
  }

  static getInstance(): PdfViewerPrivateProxy {
    return instance || (instance = new PdfViewerPrivateProxyImpl());
  }

  static setInstance(obj: PdfViewerPrivateProxy) {
    instance = obj;
  }
}

let instance: PdfViewerPrivateProxy|null = null;
