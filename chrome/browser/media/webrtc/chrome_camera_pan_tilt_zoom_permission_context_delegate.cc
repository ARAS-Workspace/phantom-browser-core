// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/chrome_camera_pan_tilt_zoom_permission_context_delegate.h"

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/compiler_specific.h"
#endif

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
#if BUILDFLAG(IS_ANDROID)
  // The PTZ permission is automatically granted on Android. It is safe to do so
  // because pan and tilt are not supported on Android.
  *content_setting_result = CONTENT_SETTING_ALLOW;
  return true;
#else
  return false;
#endif
}
