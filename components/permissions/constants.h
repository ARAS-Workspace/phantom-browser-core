// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONSTANTS_H_
#define COMPONENTS_PERMISSIONS_CONSTANTS_H_

#include <string_view>

#include "base/component_export.h"
#include "base/time/clock.h"
#include "build/build_config.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace permissions {

// The URL for the Embedded Content help center article in the SAA permission
// prompt.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kEmbeddedContentHelpCenterURL[];

// TODO(crbug.com/40254381): Remove the code related to unused site permissions
// from Android builds.

// The key used for marking permissions as revoked, as per the unused site
// permissions module of Safety Check.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kRevokedKey[];

// The key for the revoked permission type, as per the unused site permissions
// module of Safety Check
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kRevokedPermissionType[];

// The key for the revoked permission value, as per the unused site permissions
// module of Safety Check.
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const char kRevokedPermissionSettingValue[];

// How long an explicit Storage Access API permission grant/denial should last
// (not taking renewals into account).
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::TimeDelta kStorageAccessAPIExplicitPermissionLifetime;

// How long an implicit Storage Access API permission grant/denial should last
// (not taking renewals into account).
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::TimeDelta kStorageAccessAPIImplicitPermissionLifetime;

// How long a Related Website Sets Storage Access API permission
// grant/denial should last (not taking renewals into account).
COMPONENT_EXPORT(PERMISSIONS_COMMON)
extern const base::TimeDelta kStorageAccessAPIRelatedWebsiteSetsLifetime;

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONSTANTS_H_
