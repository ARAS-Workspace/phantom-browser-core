// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {HistorySignInState, SyncState} from './constants.js';

/**
 * @fileoverview Externs for objects sent from C++ to chrome://history.
 *
 * TODO(crbug.com/495832119): Remove this file, after updating consumers to use
 * the corresponding mojo-generated types (from foreign_sessions.mojom-webui.js)
 * instead.
 */
/**
 * The type of the history identity state object. This definition is based on
 * chrome/browser/ui/webui/history/history_sign_in_state_watcher.h:
 */
// LINT.IfChange(HistoryIdentityState)
export interface HistoryIdentityState {
  signIn: HistorySignInState;
  tabsSync: SyncState;
  historySync: SyncState;
}
// LINT.ThenChange(/chrome/browser/ui/webui/history/history_login_handler.cc:GetHistoryIdentityStateDict)
