// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_allowlist.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ExtensionAllowlistOmahaAttributeValue {
  kUndefined = 0,
  kAllowlisted = 1,
  kNotAllowlisted = 2,
  kMaxValue = kNotAllowlisted,
};

void ReportExtensionAllowlistOmahaAttribute(
    const base::Value* allowlist_value) {
  ExtensionAllowlistOmahaAttributeValue value;

  if (!allowlist_value) {
    value = ExtensionAllowlistOmahaAttributeValue::kUndefined;
  } else if (allowlist_value->GetBool()) {
    value = ExtensionAllowlistOmahaAttributeValue::kAllowlisted;
  } else {
    value = ExtensionAllowlistOmahaAttributeValue::kNotAllowlisted;
  }

  base::UmaHistogramEnumeration("Extensions.EsbAllowlistOmahaAttribute", value);
}

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

void ExtensionAllowlist::PerformActionBasedOnOmahaAttributes(
    const ExtensionId& extension_id,
    const base::DictValue& attributes) {
  const base::Value* allowlist_value = attributes.Find("_esbAllowlist");

  ReportExtensionAllowlistOmahaAttribute(allowlist_value);

  if (!allowlist_value) {
    // Ignore missing attribute. Omaha server should set the attribute to |true|
    // or |false|. This way the allowlist state won't flip if there is a server
    // bug where the attribute isn't sent. This will also leave external
    // extensions in the |ALLOWLIST_UNDEFINED| state.
    return;
  }

  AllowlistState allowlist_state = allowlist_value->GetBool()
                                       ? ALLOWLIST_ALLOWLISTED
                                       : ALLOWLIST_NOT_ALLOWLISTED;

  if (allowlist_state == GetExtensionAllowlistState(extension_id)) {
    // Do nothing if the state didn't change.
    return;
  }

  SetExtensionAllowlistState(extension_id, allowlist_state);
}

}  // namespace extensions
