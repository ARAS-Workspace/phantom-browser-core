// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import type {IncognitoAvailability} from './constants.js';

export interface BrowserProxy {
  getIncognitoAvailability(): Promise<IncognitoAvailability>;
  getCanEditBookmarks(): Promise<boolean>;
  getCanUploadBookmarkToAccountStorage(id: string): Promise<boolean>;
  recordInHistogram(histogram: string, bucket: number, maxBucket: number): void;
  onSingleBookmarkUploadClicked(bookmarkId: string): void;
}

export class BrowserProxyImpl implements BrowserProxy {
  getIncognitoAvailability() {
    return sendWithPromise<IncognitoAvailability>('getIncognitoAvailability');
  }

  getCanEditBookmarks() {
    return sendWithPromise<boolean>('getCanEditBookmarks');
  }

  getCanUploadBookmarkToAccountStorage(id: string) {
    return sendWithPromise<boolean>('getCanUploadBookmarkToAccountStorage', id);
  }

  recordInHistogram(histogram: string, bucket: number, maxBucket: number) {
    chrome.send(
        'metricsHandler:recordInHistogram', [histogram, bucket, maxBucket]);
  }

  onSingleBookmarkUploadClicked(bookmarkId: string) {
    chrome.send('onSingleBookmarkUploadClicked', [bookmarkId]);
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
