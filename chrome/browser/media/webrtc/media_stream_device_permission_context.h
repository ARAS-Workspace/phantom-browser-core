// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_

#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/content_setting_permission_context_base.h"

namespace permissions {
struct PermissionRequestData;
}  // namespace permissions

// Common class which handles the mic and camera permissions.
class MediaStreamDevicePermissionContext
    : public permissions::ContentSettingPermissionContextBase {
 public:
  MediaStreamDevicePermissionContext(content::BrowserContext* browser_context,
                                     ContentSettingsType content_settings_type);

  MediaStreamDevicePermissionContext(
      const MediaStreamDevicePermissionContext&) = delete;
  MediaStreamDevicePermissionContext& operator=(
      const MediaStreamDevicePermissionContext&) = delete;

  ~MediaStreamDevicePermissionContext() override;

  // PermissionContextBase:
  void DecidePermission(
      std::unique_ptr<permissions::PermissionRequestData> request_data,
      permissions::BrowserPermissionCallback callback) override;
  void ResetPermission(const GURL& requesting_origin,
                       const GURL& embedding_origin) override;

  // ContentSettingPermissionContextBase:
  // TODO(xhwang): GURL.DeprecatedGetOriginAsURL() shouldn't be used as the
  // origin. Need to refactor to use url::Origin. crbug.com/40082781 is filed
  // for this.
  ContentSetting GetContentSettingStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

 private:
  ContentSettingsType content_settings_type_;

  // Must be the last member.
  base::WeakPtrFactory<MediaStreamDevicePermissionContext> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_
