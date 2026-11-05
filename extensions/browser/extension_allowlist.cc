// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_allowlist.h"

#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Indicates whether an extension is included in the Safe Browsing allowlist.
constexpr PrefMap kPrefAllowlist = {"allowlist", PrefType::kInteger,
                                    PrefScope::kExtensionSpecific};

}  // namespace

ExtensionAllowlist::ExtensionAllowlist(content::BrowserContext* browser_context)
    : extension_prefs_(ExtensionPrefs::Get(browser_context)) {}

ExtensionAllowlist::~ExtensionAllowlist() = default;

AllowlistState ExtensionAllowlist::GetExtensionAllowlistState(
    const ExtensionId& extension_id) const {
  int value = 0;
  if (!extension_prefs_->ReadPrefAsInteger(extension_id, kPrefAllowlist,
                                           &value)) {
    return ALLOWLIST_UNDEFINED;
  }

  if (value < 0 || value > ALLOWLIST_LAST) {
    LOG(ERROR) << "Bad pref 'allowlist' for extension '" << extension_id << "'";
    return ALLOWLIST_UNDEFINED;
  }

  return static_cast<AllowlistState>(value);
}

void ExtensionAllowlist::SetExtensionAllowlistState(
    const ExtensionId& extension_id,
    AllowlistState state) {
  DCHECK_NE(state, ALLOWLIST_UNDEFINED);

  if (state != GetExtensionAllowlistState(extension_id)) {
    extension_prefs_->SetIntegerPref(extension_id, kPrefAllowlist, state);
  }
}

}  // namespace extensions
