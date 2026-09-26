// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_context.h"

#include <variant>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/permission_request_id.h"
#include "components/visibility_timer/visibility_timer_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

// static
void NotificationPermissionContext::UpdatePermission(
    content::BrowserContext* browser_context,
    const GURL& origin,
    ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_BLOCK:
    case CONTENT_SETTING_DEFAULT:
      HostContentSettingsMapFactory::GetForProfile(browser_context)
          ->SetContentSettingDefaultScope(
              origin, GURL(), ContentSettingsType::NOTIFICATIONS, setting);
      break;

    default:
      NOTREACHED();
  }
}

NotificationPermissionContext::NotificationPermissionContext(
    content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::NOTIFICATIONS,
          network::mojom::PermissionsPolicyFeature::kNotFound) {}

NotificationPermissionContext::~NotificationPermissionContext() = default;

ContentSetting NotificationPermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (render_frame_host) {
    if (render_frame_host->GetLastCommittedOrigin().opaque()) {
      return CONTENT_SETTING_BLOCK;
    }
  } else if (url::Origin::Create(requesting_origin).opaque()) {
    return CONTENT_SETTING_BLOCK;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Extensions can declare the "notifications" permission in their manifest
  // that also grant permission to use the Web Notification API.
  ContentSetting extension_status =
      GetPermissionStatusForExtension(requesting_origin);
  if (extension_status != CONTENT_SETTING_ASK)
    return extension_status;
#endif

  ContentSetting setting = permissions::ContentSettingPermissionContextBase::
      GetContentSettingStatusInternal(render_frame_host, requesting_origin,
                                      embedding_origin);

  content_settings::PageSpecificContentSettings::NotificationsAccessed(
      render_frame_host, /*blocked=*/setting != CONTENT_SETTING_ALLOW);

  if (requesting_origin != embedding_origin && setting == CONTENT_SETTING_ASK)
    return CONTENT_SETTING_BLOCK;

  return setting;
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
ContentSetting NotificationPermissionContext::GetPermissionStatusForExtension(
    const GURL& origin) const {
  constexpr ContentSetting kDefaultSetting = CONTENT_SETTING_ASK;
  if (!origin.SchemeIs(extensions::kExtensionScheme))
    return kDefaultSetting;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(
          Profile::FromBrowserContext(browser_context()))
          ->enabled_extensions()
          .GetByID(origin.GetHost());

  if (!extension || !extension->permissions_data()->HasAPIPermission(
                        extensions::mojom::APIPermissionID::kNotifications)) {
    // The |extension| doesn't exist, or doesn't have the "notifications"
    // permission declared in their manifest
    return kDefaultSetting;
  }

  NotifierStateTracker* notifier_state_tracker =
      NotifierStateTrackerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  DCHECK(notifier_state_tracker);

  message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, extension->id());
  return notifier_state_tracker->IsNotifierEnabled(notifier_id)
             ? CONTENT_SETTING_ALLOW
             : CONTENT_SETTING_BLOCK;
}
#endif

void NotificationPermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    permissions::BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Permission requests for either Web Notifications and Push Notifications may
  // only happen on top-level frames and same-origin iframes. Usage will
  // continue to be allowed in all iframes: such frames could trivially work
  // around the restriction by posting a message to their Service Worker, where
  // showing a notification is allowed.
  if (request_data->requesting_origin != request_data->embedding_origin) {
    std::move(callback).Run(content::PermissionResult(
        blink::mojom::PermissionStatus::DENIED,
        content::PermissionStatusSource::UNSPECIFIED));
    return;
  }

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data->id.global_render_frame_host_id());

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  // Notifications permission is always denied in incognito. To prevent sites
  // from using that to detect whether incognito mode is active, we deny after a
  // random time delay, to simulate a user clicking a bubble/infobar. See also
  // ContentSettingsRegistry::Init, which marks notifications as
  // INHERIT_IF_LESS_PERMISSIVE, and
  // PermissionMenuModel::PermissionMenuModel which prevents users from manually
  // allowing the permission.
  if (browser_context()->IsOffTheRecord()) {
    // Random number of seconds in the range [1.0, 2.0).
    double delay_seconds = 1.0 + 1.0 * base::RandDouble();
    visibility_timer::VisibilityTimerTabHelper::CreateForWebContents(
        web_contents);
    visibility_timer::VisibilityTimerTabHelper::FromWebContents(web_contents)
        ->PostTaskAfterVisibleDelay(
            FROM_HERE,
            base::BindOnce(
                [](base::WeakPtr<NotificationPermissionContext> context,
                   std::unique_ptr<permissions::PermissionRequestData>
                       request_data,
                   permissions::BrowserPermissionCallback callback) {
                  if (context) {
                    context->NotifyPermissionSet(
                        *request_data, std::move(callback),
                        /*persist=*/true,
                        /*permission_result=*/nullptr,
                        permissions::PermissionPromptDecision{
                            .overall_decision = PermissionDecision::kDeny,
                            .prompt_options = std::monostate(),
                            .is_final = true});
                  }
                },
                weak_factory_ui_thread_.GetWeakPtr(), std::move(request_data),
                std::move(callback)),
            base::Seconds(delay_seconds));
    return;
  }

  permissions::ContentSettingPermissionContextBase::DecidePermission(
      std::move(request_data), std::move(callback));
}

void NotificationPermissionContext::UpdateTabContext(
    const permissions::PermissionRequestData& request_data,
    bool allowed) {
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          request_data.id.global_render_frame_host_id());
  if (!content_settings) {
    return;
  }

  if (allowed) {
    content_settings->OnContentAllowed(ContentSettingsType::NOTIFICATIONS);
  } else {
    content_settings->OnContentBlocked(ContentSettingsType::NOTIFICATIONS);
  }
}
