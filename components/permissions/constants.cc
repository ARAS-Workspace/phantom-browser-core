// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/constants.h"
#include "base/time/clock.h"

namespace permissions {

const char kEmbeddedContentHelpCenterURL[] =
    "https://support.google.com/chrome/?p=embedded_content";

// TODO(crbug.com/40254381): Remove the code related to unused site permissions
// from Android builds.

const char kRevokedKey[] = "revoked";
const char kRevokedPermissionType[] = "revoked_type";
const char kRevokedPermissionSettingValue[] = "revoked_value";

const base::TimeDelta kStorageAccessAPIExplicitPermissionLifetime =
    base::Days(30);

const base::TimeDelta kStorageAccessAPIImplicitPermissionLifetime =
    base::Hours(24);

const base::TimeDelta kStorageAccessAPIRelatedWebsiteSetsLifetime =
    base::Days(30);

}  // namespace permissions
