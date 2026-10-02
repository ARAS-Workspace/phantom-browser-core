// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_ENCRYPTION_CACHE_PREFS_H_
#define COMPONENTS_ENTERPRISE_ENCRYPTION_CACHE_PREFS_H_

class PrefRegistrySimple;

namespace enterprise_connectors {

// Pref that maps to the "CacheEncryptionEnabled" policy.
extern const char kCacheEncryptionEnabledPref[];

// Pref that for storing the primary key used for encrypting the HTTP cache.
// This key is stored in the user's profile preferences and is itself encrypted.
extern const char kEncryptedCachePrimaryKey[];

void RegisterCacheEncryptionProfilePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_ENCRYPTION_CACHE_PREFS_H_
