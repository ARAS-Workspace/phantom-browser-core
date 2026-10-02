// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOOKALIKES_CORE_URL_PATTERN_UTIL_H_
#define COMPONENTS_LOOKALIKES_CORE_URL_PATTERN_UTIL_H_

#include <string>
#include <vector>

class GURL;

namespace lookalikes {

// Canonicalizes `url` for pattern matching and writes its non-empty hostname,
// path and query to the corresponding non-null out-parameters. `url` must be
// valid; nothing is written for a non-standard `url`.
void CanonicalizeUrl(const GURL& url,
                     std::string* canonicalized_hostname,
                     std::string* canonicalized_path,
                     std::string* canonicalized_query);

// Clears `hosts`; for a non-empty `host`, fills it with up to 4 host suffixes
// (the last component is never examined alone) followed by `host` itself.
// Note: `host` must be canonicalized before calling this method.
void GenerateHostVariantsToCheck(const std::string& host,
                                 std::vector<std::string>* hosts);

// Clears `paths`; for a non-empty `path`, fills it with up to 4 prefixes of
// `path` that end in '/', then `path` itself when a prefix was found and it
// differs from the last one, then `path` + "?" + `query` when `query` is not
// empty. Note: `path` must be canonicalized before calling this method.
void GeneratePathVariantsToCheck(const std::string& path,
                                 const std::string& query,
                                 std::vector<std::string>* paths);

}  // namespace lookalikes

#endif  // COMPONENTS_LOOKALIKES_CORE_URL_PATTERN_UTIL_H_
