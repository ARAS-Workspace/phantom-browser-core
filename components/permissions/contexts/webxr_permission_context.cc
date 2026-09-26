// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/webxr_permission_context.h"

#include <memory>
#include <variant>

#include "base/check.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/permission_request_data.h"
#include "device/vr/buildflags/buildflags.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {
WebXrPermissionContext::WebXrPermissionContext(
    content::BrowserContext* browser_context,
    ContentSettingsType content_settings_type)
    : ContentSettingPermissionContextBase(
          browser_context,
          content_settings_type,
          network::mojom::PermissionsPolicyFeature::kWebXr),
      content_settings_type_(content_settings_type) {
  DCHECK(content_settings_type_ == ContentSettingsType::VR ||
         content_settings_type_ == ContentSettingsType::AR ||
         content_settings_type_ == ContentSettingsType::HAND_TRACKING);
}

WebXrPermissionContext::~WebXrPermissionContext() = default;

}  // namespace permissions
