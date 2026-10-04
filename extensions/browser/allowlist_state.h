// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ALLOWLIST_STATE_H_
#define EXTENSIONS_BROWSER_ALLOWLIST_STATE_H_

namespace extensions {

// The Safe Browsing extension allowlist states.
enum AllowlistState {
  // The allowlist state is unknown.
  ALLOWLIST_UNDEFINED = 0,
  // The extension is included in the Safe Browsing extension allowlist.
  ALLOWLIST_ALLOWLISTED = 1,
  // The extension is not included in the Safe Browsing extension allowlist.
  ALLOWLIST_NOT_ALLOWLISTED = 2,
  ALLOWLIST_LAST = 2
};

// The acknowledge states for the Safe Browsing CRX allowlist enforcements.
enum AllowlistAcknowledgeState {
  ALLOWLIST_ACKNOWLEDGE_NONE = 0,
  ALLOWLIST_ACKNOWLEDGE_NEEDED = 1,
  ALLOWLIST_ACKNOWLEDGE_DONE = 2,
  ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER = 3,
  ALLOWLIST_ACKNOWLEDGE_LAST = 3
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ALLOWLIST_STATE_H_
