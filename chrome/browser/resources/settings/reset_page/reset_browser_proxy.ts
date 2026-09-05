// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

export interface ResetBrowserProxy {
  /**
   * @return A promise firing once resetting has completed.
   */
  performResetProfileSettings(): Promise<void>;

  /**
   * A method to be called when the reset profile banner is hidden.
   */
  onHideResetProfileBanner(): void;

  /**
   * Retrieves the triggered reset tool name.
   * @return A promise firing with the tool name, once it has been retrieved.
   */
  getTriggeredResetToolName(): Promise<string>;

  /**
   * @return A method that retrieves the list of tampered prefs.
   */
  getTamperedPreferencePaths(): Promise<string[]>;
}

export class ResetBrowserProxyImpl implements ResetBrowserProxy {
  performResetProfileSettings() {
    return sendWithPromise<void>('performResetProfileSettings');
  }

  onHideResetProfileBanner() {
    chrome.send('onHideResetProfileBanner');
  }

  getTriggeredResetToolName(): Promise<string> {
    return sendWithPromise<string>('getTriggeredResetToolName');
  }

  getTamperedPreferencePaths(): Promise<string[]> {
    return sendWithPromise<string[]>('getTamperedPreferencePaths');
  }

  static getInstance(): ResetBrowserProxy {
    return instance || (instance = new ResetBrowserProxyImpl());
  }

  static setInstance(obj: ResetBrowserProxy) {
    instance = obj;
  }
}

let instance: ResetBrowserProxy|null = null;
