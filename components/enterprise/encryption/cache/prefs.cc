// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/encryption/cache/prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace enterprise_connectors {

const char kCacheEncryptionEnabledPref[] =
    "enterprise_connectors.cache_encryption_enabled";
const char kEncryptedCachePrimaryKey[] =
    "enterprise.encrypted_cache_primary_key";

void RegisterCacheEncryptionProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kCacheEncryptionEnabledPref, false);
  registry->RegisterStringPref(kEncryptedCachePrimaryKey, "");
}

}  // namespace enterprise_connectors
