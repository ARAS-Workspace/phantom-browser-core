// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_WEBXR_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_WEBXR_PERMISSION_CONTEXT_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_request_data.h"

namespace content {
struct PermissionResult;
}  // namespace content

namespace permissions {
struct PermissionPromptDecision;

class WebXrPermissionContext : public ContentSettingPermissionContextBase {
 public:
  WebXrPermissionContext(content::BrowserContext* browser_context,
                         ContentSettingsType content_settings_type);
  ~WebXrPermissionContext() override;
  WebXrPermissionContext(const WebXrPermissionContext&) = delete;
  WebXrPermissionContext& operator=(const WebXrPermissionContext&) = delete;

 private:
  // PermissionContextBase:

  ContentSettingsType content_settings_type_;

  // Must be the last member, to ensure that it will be
  // destroyed first, which will invalidate weak pointers
  base::WeakPtrFactory<WebXrPermissionContext> weak_ptr_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_WEBXR_PERMISSION_CONTEXT_H_
