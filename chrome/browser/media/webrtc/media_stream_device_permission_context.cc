// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_device_permission_context.h"

#include "base/command_line.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/suggest_permission_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#endif

namespace {

network::mojom::PermissionsPolicyFeature GetPermissionsPolicyFeature(
    ContentSettingsType type) {
  if (type == ContentSettingsType::MEDIASTREAM_MIC) {
    return network::mojom::PermissionsPolicyFeature::kMicrophone;
  }

  DCHECK_EQ(ContentSettingsType::MEDIASTREAM_CAMERA, type);
  return network::mojom::PermissionsPolicyFeature::kCamera;
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
void CallbackPermissionStatusWrapper(
    base::OnceCallback<void(content::PermissionResult)> callback,
    bool allowed) {
  std::move(callback).Run(content::PermissionResult(
      allowed ? blink::mojom::PermissionStatus::GRANTED
              : blink::mojom::PermissionStatus::DENIED,
      content::PermissionStatusSource::UNSPECIFIED));
}
#endif

}  // namespace

MediaStreamDevicePermissionContext::MediaStreamDevicePermissionContext(
    content::BrowserContext* browser_context,
    const ContentSettingsType content_settings_type)
    : permissions::ContentSettingPermissionContextBase(
          browser_context,
          content_settings_type,
          GetPermissionsPolicyFeature(content_settings_type)),
      content_settings_type_(content_settings_type) {
  DCHECK(content_settings_type_ == ContentSettingsType::MEDIASTREAM_MIC ||
         content_settings_type_ == ContentSettingsType::MEDIASTREAM_CAMERA);
}

MediaStreamDevicePermissionContext::~MediaStreamDevicePermissionContext() =
    default;

ContentSetting
MediaStreamDevicePermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // TODO(raymes): Merge this policy check into content settings
  // crbug.com/41014586.
  const char* policy_name = nullptr;
  const char* urls_policy_name = nullptr;
  if (content_settings_type_ == ContentSettingsType::MEDIASTREAM_MIC) {
    policy_name = prefs::kAudioCaptureAllowed;
    urls_policy_name = prefs::kAudioCaptureAllowedUrls;
  } else {
    DCHECK(content_settings_type_ == ContentSettingsType::MEDIASTREAM_CAMERA);
    policy_name = prefs::kVideoCaptureAllowed;
    urls_policy_name = prefs::kVideoCaptureAllowedUrls;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream)) {
    bool blocked = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                       switches::kUseFakeUIForMediaStream) == "deny";
    return blocked ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW;
  }

  MediaStreamDevicePolicy policy =
      GetDevicePolicy(Profile::FromBrowserContext(browser_context()),
                      requesting_origin, policy_name, urls_policy_name);

  switch (policy) {
    case ALWAYS_DENY:
      return CONTENT_SETTING_BLOCK;
    case ALWAYS_ALLOW:
      return CONTENT_SETTING_ALLOW;
    default:
      DCHECK_EQ(POLICY_NOT_SET, policy);
  }

  // Check the content setting. TODO(raymes): currently mic/camera permission
  // doesn't consider the embedder.
  ContentSetting setting = permissions::ContentSettingPermissionContextBase::
      GetContentSettingStatusInternal(render_frame_host, requesting_origin,
                                      requesting_origin);

  if (setting == CONTENT_SETTING_DEFAULT) {
    setting = CONTENT_SETTING_ASK;
  }

  return setting;
}

void MediaStreamDevicePermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    permissions::BrowserPermissionCallback callback) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data->id.global_render_frame_host_id());
  if (rfh) {
    extensions::WebViewPermissionHelper* web_view_permission_helper =
        extensions::WebViewPermissionHelper::FromRenderFrameHost(rfh);
    if (web_view_permission_helper) {
      // TODO(crbug.com/521370750): This is part of the plumbing to support
      // PEPC inside <webview> guests. Track full support/enablement here.
      web_view_permission_helper->RequestMediaPermission(
          content_settings_type_, request_data->requesting_origin,
          request_data->user_gesture,
          base::BindOnce(&CallbackPermissionStatusWrapper,
                         std::move(callback)));
      return;
    }

    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(browser_context());
    url::Origin requesting_origin = rfh->GetLastCommittedOrigin();
    const extensions::Extension* extension =
        extension_registry->enabled_extensions().GetExtensionOrAppByURL(
            requesting_origin.GetURL());
    if (extension) {
      extensions::mojom::APIPermissionID permission_id =
          (content_settings_type_ == ContentSettingsType::MEDIASTREAM_MIC)
              ? extensions::mojom::APIPermissionID::kAudioCapture
              : extensions::mojom::APIPermissionID::kVideoCapture;
      bool has_permission = false;
      if (extension->is_platform_app()) {
        has_permission =
            extensions::IsExtensionWithPermissionOrSuggestInConsole(
                permission_id, extension, rfh->GetMainFrame());
      } else {
        has_permission =
            extension->permissions_data()->HasAPIPermission(permission_id);
      }
      if (has_permission) {
        if (extensions::ProcessMap::Get(browser_context())
                ->Contains(
                    extension->id(),
                    request_data->id.global_render_frame_host_id().child_id)) {
          std::move(callback).Run(content::PermissionResult(
              blink::mojom::PermissionStatus::GRANTED,
              content::PermissionStatusSource::UNSPECIFIED));
          return;
        }
      }
    }
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  permissions::ContentSettingPermissionContextBase::DecidePermission(
      std::move(request_data), std::move(callback));
}

void MediaStreamDevicePermissionContext::ResetPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  NOTREACHED() << "ResetPermission is not implemented";
}
