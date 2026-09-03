// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

// WARNING: Keep synced with
// chrome/browser/ui/webui/password_manager/sync_handler.cc.
export enum TrustedVaultBannerState {
  NOT_SHOWN = 0,
  OFFER_OPT_IN = 1,
  OPTED_IN = 2,
}

/**
 * @see chrome/browser/ui/webui/password_manager/sync_handler.cc
 */
export interface AccountInfo {
  email: string;
  avatarImage?: string;
}

export interface SyncInfo {
  isSyncingPasswords: boolean;
}

export interface SyncBrowserProxy {
  /**
   * Gets trusted vault banner state.
   */
  getTrustedVaultBannerState(): Promise<TrustedVaultBannerState>;

  /**
   * Gets the current sync info.
   */
  getSyncInfo(): Promise<SyncInfo>;

  /**
   * Gets the current account info.
   */
  getAccountInfo(): Promise<AccountInfo>;

  /**
   * Returns the number of local data password from the sync service API.
   */
  getLocalPasswordCount(): Promise<number>;

  /**
   * Opens the batch upload dialog on top of the current page with the passwords
   * entry point as input.
   */
}

export class SyncBrowserProxyImpl implements SyncBrowserProxy {
  getTrustedVaultBannerState() {
    return sendWithPromise<TrustedVaultBannerState>(
        'GetSyncTrustedVaultBannerState');
  }

  getSyncInfo() {
    return sendWithPromise<SyncInfo>('GetSyncInfo');
  }

  getAccountInfo() {
    return sendWithPromise<AccountInfo>('GetAccountInfo');
  }

  getLocalPasswordCount() {
    return sendWithPromise<number>('GetLocalPasswordCount');
  }

  static getInstance(): SyncBrowserProxy {
    return instance || (instance = new SyncBrowserProxyImpl());
  }

  static setInstance(obj: SyncBrowserProxy) {
    instance = obj;
  }
}

let instance: SyncBrowserProxy|null = null;
