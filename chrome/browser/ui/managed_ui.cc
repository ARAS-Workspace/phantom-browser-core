// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include <optional>

#include "build/build_config.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

bool ShouldDisplayManagedByParentUi(Profile* profile) {
  return profile && profile->IsChild();
}

}  // namespace

bool ShouldDisplayManagedUi(Profile* profile) {

  return enterprise_util::IsBrowserManaged(profile) ||
         ShouldDisplayManagedByParentUi(profile);
}

const gfx::VectorIcon& GetManagedUiIcon(Profile* profile) {
  CHECK(ShouldDisplayManagedUi(profile));

  if (enterprise_util::IsBrowserManaged(profile)) {
    return features::IsRoundedIconsEnabled()
               ? vector_icons::kDomainIcon
               : vector_icons::kBusinessChromeRefreshOldIcon;
  }

  CHECK(ShouldDisplayManagedByParentUi(profile));
  return features::IsRoundedIconsEnabled() ? vector_icons::kFamilyLinkIcon
                                           : vector_icons::kFamilyLinkOldIcon;
}
