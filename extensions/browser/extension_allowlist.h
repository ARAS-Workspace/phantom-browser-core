// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ALLOWLIST_H_
#define EXTENSIONS_BROWSER_EXTENSION_ALLOWLIST_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace base {
class DictValue;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Manages the Safe Browsing CRX Allowlist.
class ExtensionAllowlist : public KeyedService {
 public:
  ExtensionAllowlist(const ExtensionAllowlist&) = delete;
  ExtensionAllowlist& operator=(const ExtensionAllowlist&) = delete;
  ~ExtensionAllowlist() override;

  // Gets the Safe Browsing allowlist state.
  AllowlistState GetExtensionAllowlistState(
      const ExtensionId& extension_id) const;

  // Sets the Safe Browsing allowlist state.
  void SetExtensionAllowlistState(const ExtensionId& extension_id,
                                  AllowlistState state);

  // Performs action based on Omaha attributes for the extension.
  void PerformActionBasedOnOmahaAttributes(const ExtensionId& extension_id,
                                           const base::DictValue& attributes);

 private:
  friend class ExtensionAllowlistFactory;

  // `browser_context` must outlive this object and the ownership remains at
  // caller.
  explicit ExtensionAllowlist(content::BrowserContext* browser_context);

  raw_ptr<ExtensionPrefs> extension_prefs_ = nullptr;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ALLOWLIST_H_
