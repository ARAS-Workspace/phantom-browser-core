// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_extensions_allowlist/allowlist.h"

#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

bool IsComponentExtensionAllowlisted(const std::string& extension_id) {
  constexpr auto kAllowed = base::MakeFixedFlatSet<std::string_view>({
      extension_misc::kInAppPaymentsSupportAppId,
      extension_misc::kPdfExtensionId,
      extension_misc::kDictationConnectorExtensionId,
  });

  if (kAllowed.contains(extension_id)) {
    return true;
  }

  LOG(ERROR) << "Component extension with id " << extension_id << " not in "
             << "allowlist and is not being loaded as a result.";
  NOTREACHED() << "Component extension with id " << extension_id << " not in "
               << "allowlist and is not being loaded as a result.";
}

}  // namespace extensions
