// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {IconLoader, MojomData, PageHandlerInterface} from 'chrome://downloads/downloads.js';
import {DangerType, State} from 'chrome://downloads/downloads.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class FakePageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  constructor() {
    super([
      'discardDangerous',
      'remove',
      'saveSuspiciousRequiringGesture',
    ]);
  }

  remove(id: string) {
    this.methodCalled('remove', id);
  }

  discardDangerous(id: string) {
    this.methodCalled('discardDangerous', id);
  }

  saveSuspiciousRequiringGesture(id: string) {
    this.methodCalled('saveSuspiciousRequiringGesture', id);
  }

  getDownloads(_searchTerms: string[]) {}
  openFileRequiringGesture(_id: string) {}
  drag(_id: string) {}
  retryDownload(_id: string) {}
  show(_id: string) {}
  pause(_id: string) {}
  resume(_id: string) {}
  undo() {}
  cancel(_id: string) {}
  clearAll() {}
  openDownloadsFolderRequiringGesture() {}
  reviewDangerousRequiringGesture(_id: string) {}
}

export class TestIconLoader extends TestBrowserProxy implements IconLoader {
  private shouldIconsLoad_: boolean = true;

  constructor() {
    super(['loadIcon']);
  }

  setShouldIconsLoad(shouldIconsLoad: boolean) {
    this.shouldIconsLoad_ = shouldIconsLoad;
  }

  loadIcon(_imageEl: HTMLImageElement, filePath: string) {
    this.methodCalled('loadIcon', filePath);
    return Promise.resolve(this.shouldIconsLoad_);
  }
}

export function createDownload(config?: Partial<MojomData>): MojomData {
  return Object.assign(
      {
        byExtId: '',
        byExtName: '',
        dangerType: DangerType.kNoApplicableDangerType,
        dateString: '',
        fileExternallyRemoved: false,
        fileName: 'download 1',
        filePath: '/some/file/path',
        fileUrl: 'file:///some/file/path',
        hideDate: false,
        id: '123',
        isInsecure: false,
        isReviewable: false,
        lastReasonText: '',
        otr: false,
        percent: 100,
        progressStatusText: '',
        resume: false,
        retry: false,
        return: false,
        shouldShowIncognitoWarning: false,
        showInFolderText: '',
        sinceString: 'Today',
        started: Date.now() - 10000,
        state: State.kComplete,
        total: -1,
        url: 'http://permission.site',
        displayInitiatorOrigin: 'http://permission.site',
      },
      config || {});
}
