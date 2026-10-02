// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safety_check/safety_check.h"


namespace {

const base::TimeDelta kUnusedSitePermissionsRevocationCleanUpThreshold =
    base::Days(30);

}  // namespace

namespace safety_check {

base::TimeDelta GetUnusedSitePermissionsRevocationCleanUpThreshold() {
  return kUnusedSitePermissionsRevocationCleanUpThreshold;
}

SafeBrowsingStatus CheckSafeBrowsing(PrefService* pref_service) {
  return SafeBrowsingStatus::kDisabled;
}

}  // namespace safety_check
