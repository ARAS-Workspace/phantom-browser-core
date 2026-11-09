// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_url_constants.h"

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extension_permissions_constants {

// The link to the site permissions settings page.
const char kExtensionsSitePermissionsURL[] =
    "chrome://extensions/sitePermissions";

}  // namespace extension_permissions_constants
