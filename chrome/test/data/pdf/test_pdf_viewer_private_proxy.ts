// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import type {PdfViewerPrivateProxy} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPdfViewerPrivateProxy extends TestBrowserProxy implements
    PdfViewerPrivateProxy {
  private getTextInfoResult_: chrome.pdfViewerPrivate.GetTextInfoResult = {
    typefaces: [],
    mojoTextInfo: new ArrayBuffer(0),
  };

  constructor() {
    super([
      'getTextInfo',
      'setPdfDocumentTitle',
    ]);
  }


  // <if expr="enable_pdf_ink2">
  setGetTextInfoResult(result: chrome.pdfViewerPrivate.GetTextInfoResult) {
    this.getTextInfoResult_ = result;
  }

  getTextInfo(textarea: HTMLTextAreaElement, knownFontIds: number[]):
      Promise<chrome.pdfViewerPrivate.GetTextInfoResult> {
    this.methodCalled('getTextInfo', {textarea, knownFontIds});
    return Promise.resolve(this.getTextInfoResult_);
  }
  // </if>

  setPdfDocumentTitle(title: string): void {
    this.methodCalled('setPdfDocumentTitle', title);
  }
}
