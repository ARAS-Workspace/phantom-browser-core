// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/chrome_camera_pan_tilt_zoom_permission_context_delegate.h"

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "url/origin.h"

ChromeCameraPanTiltZoomPermissionContextDelegate::
    ChromeCameraPanTiltZoomPermissionContextDelegate(
        content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ChromeCameraPanTiltZoomPermissionContextDelegate::
    ~ChromeCameraPanTiltZoomPermissionContextDelegate() = default;

bool ChromeCameraPanTiltZoomPermissionContextDelegate::
    GetPermissionStatusInternal(const GURL& requesting_origin,
                                const GURL& embedding_origin,
                                ContentSetting* content_setting_result) {
  return false;
}
