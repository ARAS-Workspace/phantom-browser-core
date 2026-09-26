// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/request_type.h"

#include <algorithm>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permissions_client.h"
#include "ui/base/ui_base_features.h"

#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/vector_icon_types.h"

namespace permissions {

namespace {

// TODO(crbug.com/335848275): Migrate the icons in 2 steps.
// 1 - Copy contents of refresh icons into current non-refresh icons.
// 2 - In a separate change, remove the refresh icons.
const gfx::VectorIcon& GetIconIdDesktop(RequestType type) {
  switch (type) {
    case RequestType::kArSession:
    case RequestType::kVrSession:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kCardboardIcon
                 : vector_icons::kVrHeadsetChromeRefreshOldIcon;
    case RequestType::kCameraPanTiltZoom:
    case RequestType::kCameraStream:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kVideocamIcon
                 : vector_icons::kVideocamChromeRefreshOldIcon;
    case RequestType::kCapturedSurfaceControl:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kTouchpadMouseIcon
                 : vector_icons::kTouchpadMouseOldIcon;
    case RequestType::kClipboard:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kContentPasteIcon
                 : vector_icons::kContentPasteOldIcon;
    case RequestType::kDiskQuota:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kFolderFlippableIcon
                 : vector_icons::kFolderChromeRefreshOldIcon;
    case RequestType::kGeolocation:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kLocationOnIcon
                 : vector_icons::kLocationOnChromeRefreshOldIcon;
    case RequestType::kHandTracking:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kHandGestureIcon
                 : vector_icons::kHandGestureOldIcon;
    case RequestType::kIdleDetection:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kDevicesIcon
                 : vector_icons::kDevicesOldIcon;
    case RequestType::kKeyboardLock:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kKeyboardLockIcon
                 : vector_icons::kKeyboardLockOldIcon;
    case RequestType::kLocalFonts:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kFontDownloadIcon
                 : vector_icons::kFontDownloadChromeRefreshOldIcon;
    case RequestType::kLocalNetwork:
      return ::features::IsRoundedIconsEnabled() ? vector_icons::kRouterIcon
                                                 : vector_icons::kRouterOldIcon;
    case RequestType::kLoopbackNetwork:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kDesktopWindowsIcon
                 : vector_icons::kDesktopWindowsOldIcon;
    case RequestType::kMicStream:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kMicIcon
                 : vector_icons::kMicChromeRefreshOldIcon;
    case RequestType::kMidiSysex:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kPianoIcon
                 : vector_icons::kMidiChromeRefreshOldIcon;
    case RequestType::kMultipleDownloads:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kDownloadIcon
                 : vector_icons::kFileDownloadChromeRefreshOldIcon;
    case RequestType::kNotifications:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kNotificationsIcon
                 : vector_icons::kNotificationsChromeRefreshOldIcon;
    case RequestType::kPointerLock:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kMouseLockIcon
                 : vector_icons::kPointerLockOldIcon;
    case RequestType::kRegisterProtocolHandler:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kProtocolHandlerIcon
                 : vector_icons::kProtocolHandlerOldIcon;
    case RequestType::kSensors:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kSensorsIcon
                 : vector_icons::kSensorsChromeRefreshOldIcon;
    case RequestType::kWebAppInstallation:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kInstallDesktopIcon
                 : vector_icons::kInstallDesktopOldIcon;
    case RequestType::kStorageAccess:
    case RequestType::kTopLevelStorageAccess:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kVr180Create2dIcon
                 : vector_icons::kStorageAccessOldIcon;
    case RequestType::kWindowManagement:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kSelectWindowIcon
                 : vector_icons::kSelectWindowChromeRefreshOldIcon;
    case RequestType::kFileSystemAccess:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kFolderFilledIcon
                 : vector_icons::kFolderOldIcon;
    case RequestType::kIdentityProvider:
      // TODO(crbug.com/40252825): provide a dedicated icon.
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kFolderFilledIcon
                 : vector_icons::kFolderOldIcon;
  }
  NOTREACHED();
}

const gfx::VectorIcon& GetBlockedIconIdDesktop(RequestType type) {
  switch (type) {
    case RequestType::kGeolocation:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kLocationOffIcon
                 : vector_icons::kLocationOffChromeRefreshOldIcon;
    case RequestType::kNotifications:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kNotificationsOffIcon
                 : vector_icons::kNotificationsOffChromeRefreshOldIcon;
    case RequestType::kArSession:
    case RequestType::kVrSession:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kCardboardOffIcon
                 : vector_icons::kVrHeadsetOffChromeRefreshOldIcon;
    case RequestType::kCameraStream:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kVideocamOffIcon
                 : vector_icons::kVideocamOffChromeRefreshOldIcon;
    case RequestType::kCapturedSurfaceControl:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kTouchpadMouseOffIcon
                 : vector_icons::kTouchpadMouseOffOldIcon;
    case RequestType::kClipboard:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kContentPasteOffIcon
                 : vector_icons::kContentPasteOffOldIcon;
    case RequestType::kHandTracking:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kHandGestureOffIcon
                 : vector_icons::kHandGestureOffOldIcon;
    case RequestType::kIdleDetection:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kDevicesOffIcon
                 : vector_icons::kDevicesOffOldIcon;
    case RequestType::kLocalNetwork:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kRouterOffIcon
                 : vector_icons::kRouterOffOldIcon;
    case RequestType::kLoopbackNetwork:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kDesktopAccessDisabledIcon
                 : vector_icons::kDesktopAccessDisabledOldIcon;
    case RequestType::kMicStream:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kMicOffIcon
                 : vector_icons::kMicOffChromeRefreshOldIcon;
    case RequestType::kMidiSysex:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kPianoOffIcon
                 : vector_icons::kMidiOffChromeRefreshOldIcon;
    case RequestType::kSensors:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kSensorsOffIcon
                 : vector_icons::kSensorsOffChromeRefreshOldIcon;
    case RequestType::kStorageAccess:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kVr180Create2dOffIcon
                 : vector_icons::kStorageAccessOffOldIcon;
    case RequestType::kIdentityProvider:
      // TODO(crbug.com/40252825): use a dedicated icon
      return gfx::VectorIcon::EmptyIcon();
    case RequestType::kKeyboardLock:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kKeyboardLockOffIcon
                 : vector_icons::kKeyboardLockOffOldIcon;
    case RequestType::kPointerLock:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kMouseLockOffIcon
                 : vector_icons::kPointerLockOffOldIcon;
    case RequestType::kWebAppInstallation:
      return vector_icons::kInstallDesktopOffCustomIcon;
    default:
      NOTREACHED();
  }
}

}  // namespace

bool IsRequestablePermissionType(ContentSettingsType content_settings_type) {
  return !!ContentSettingsTypeToRequestTypeIfExists(content_settings_type);
}

std::optional<RequestType> ContentSettingsTypeToRequestTypeIfExists(
    ContentSettingsType content_settings_type) {
  switch (content_settings_type) {
    case ContentSettingsType::AR:
      return RequestType::kArSession;
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      return RequestType::kCameraPanTiltZoom;
    case ContentSettingsType::CAPTURED_SURFACE_CONTROL:
      return RequestType::kCapturedSurfaceControl;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return RequestType::kCameraStream;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      return RequestType::kClipboard;
    case ContentSettingsType::LOCAL_FONTS:
      return RequestType::kLocalFonts;
    case ContentSettingsType::GEOLOCATION:
    case ContentSettingsType::GEOLOCATION_WITH_OPTIONS:
      return RequestType::kGeolocation;
    case ContentSettingsType::HAND_TRACKING:
      return RequestType::kHandTracking;
    case ContentSettingsType::IDLE_DETECTION:
      return RequestType::kIdleDetection;
    case ContentSettingsType::KEYBOARD_LOCK:
      return RequestType::kKeyboardLock;
    case ContentSettingsType::MEDIASTREAM_MIC:
      return RequestType::kMicStream;
    case ContentSettingsType::MIDI_SYSEX:
      return RequestType::kMidiSysex;
    case ContentSettingsType::NOTIFICATIONS:
      return RequestType::kNotifications;
    case ContentSettingsType::SENSORS:
      return RequestType::kSensors;
    case ContentSettingsType::POINTER_LOCK:
      return RequestType::kPointerLock;
    case ContentSettingsType::STORAGE_ACCESS:
      return RequestType::kStorageAccess;
    case ContentSettingsType::VR:
      return RequestType::kVrSession;
    case ContentSettingsType::WINDOW_MANAGEMENT:
      return RequestType::kWindowManagement;
    case ContentSettingsType::LOCAL_NETWORK:
      return RequestType::kLocalNetwork;
    case ContentSettingsType::LOOPBACK_NETWORK:
      return RequestType::kLoopbackNetwork;
    case ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS:
      return RequestType::kTopLevelStorageAccess;
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      return RequestType::kFileSystemAccess;
    case ContentSettingsType::FEDERATED_IDENTITY_API:
      return RequestType::kIdentityProvider;
    default:
      return std::nullopt;
    case ContentSettingsType::WEB_APP_INSTALLATION:
      return RequestType::kWebAppInstallation;
  }
}

RequestType ContentSettingsTypeToRequestType(
    ContentSettingsType content_settings_type) {
  std::optional<RequestType> request_type =
      ContentSettingsTypeToRequestTypeIfExists(content_settings_type);
  CHECK(request_type);
  return *request_type;
}

std::optional<ContentSettingsType> RequestTypeToContentSettingsType(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kArSession:
      return ContentSettingsType::AR;
    case RequestType::kCameraPanTiltZoom:
      return ContentSettingsType::CAMERA_PAN_TILT_ZOOM;
    case RequestType::kCameraStream:
      return ContentSettingsType::MEDIASTREAM_CAMERA;
    case RequestType::kCapturedSurfaceControl:
      return ContentSettingsType::CAPTURED_SURFACE_CONTROL;
    case RequestType::kClipboard:
      return ContentSettingsType::CLIPBOARD_READ_WRITE;
    case RequestType::kLocalFonts:
      return ContentSettingsType::LOCAL_FONTS;
    case RequestType::kLocalNetwork:
      return ContentSettingsType::LOCAL_NETWORK;
    case RequestType::kLoopbackNetwork:
      return ContentSettingsType::LOOPBACK_NETWORK;
    case RequestType::kGeolocation:
      return content_settings::GeolocationContentSettingsType();
    case RequestType::kHandTracking:
      return ContentSettingsType::HAND_TRACKING;
    case RequestType::kIdleDetection:
      return ContentSettingsType::IDLE_DETECTION;
    case RequestType::kKeyboardLock:
      return ContentSettingsType::KEYBOARD_LOCK;
    case RequestType::kMicStream:
      return ContentSettingsType::MEDIASTREAM_MIC;
    case RequestType::kMidiSysex:
      return ContentSettingsType::MIDI_SYSEX;
    case RequestType::kNotifications:
      return ContentSettingsType::NOTIFICATIONS;
    case RequestType::kSensors:
      return ContentSettingsType::SENSORS;
    case RequestType::kPointerLock:
      return ContentSettingsType::POINTER_LOCK;
    case RequestType::kStorageAccess:
      return ContentSettingsType::STORAGE_ACCESS;
    case RequestType::kVrSession:
      return ContentSettingsType::VR;
    case RequestType::kWindowManagement:
      return ContentSettingsType::WINDOW_MANAGEMENT;
    case RequestType::kTopLevelStorageAccess:
      return ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS;
    case RequestType::kWebAppInstallation:
      return ContentSettingsType::WEB_APP_INSTALLATION;
    case RequestType::kDiskQuota:
    case RequestType::kFileSystemAccess:
    case RequestType::kIdentityProvider:
    case RequestType::kMultipleDownloads:
    case RequestType::kRegisterProtocolHandler:
      return std::nullopt;
  }
}

// Returns whether confirmation chips can be displayed
bool IsConfirmationChipSupported(RequestType for_request_type) {
  static constexpr auto kRequestsWithChip =
      base::MakeFixedFlatSet<RequestType>({
          // clang-format off
          RequestType::kNotifications,
          RequestType::kGeolocation,
          RequestType::kCameraStream,
          RequestType::kMicStream,
          RequestType::kSensors,
          // clang-format on
      });
  return kRequestsWithChip.contains(for_request_type);
}

IconId GetIconId(RequestType type) {
  IconId override_id = PermissionsClient::Get()->GetOverrideIconId(type);
  if (!override_id.is_empty()) {
    return override_id;
  }
  return GetIconIdDesktop(type);
}

IconId GetBlockedIconId(RequestType type) {
  return GetBlockedIconIdDesktop(type);
}

const char* PermissionKeyForRequestType(permissions::RequestType request_type) {
  switch (request_type) {
    case permissions::RequestType::kArSession:
      return "ar_session";
    case permissions::RequestType::kCameraPanTiltZoom:
      return "camera_pan_tilt_zoom";
    case permissions::RequestType::kCameraStream:
      return "camera_stream";
    case permissions::RequestType::kCapturedSurfaceControl:
      return "captured_surface_control";
    case permissions::RequestType::kClipboard:
      return "clipboard";
    case permissions::RequestType::kDiskQuota:
      return "disk_quota";
    case permissions::RequestType::kFileSystemAccess:
      return "file_system";
    case permissions::RequestType::kGeolocation:
      return "geolocation";
    case permissions::RequestType::kHandTracking:
      return "hand_tracking";
    case permissions::RequestType::kIdleDetection:
      return "idle_detection";
    case permissions::RequestType::kKeyboardLock:
      return "keyboard_lock";
    case permissions::RequestType::kLocalFonts:
      return "local_fonts";
    case permissions::RequestType::kLocalNetwork:
      return "local_network";
    case permissions::RequestType::kLoopbackNetwork:
      return "loopback_network";
    case permissions::RequestType::kMicStream:
      return "mic_stream";
    case permissions::RequestType::kMidiSysex:
      return "midi_sysex";
    case permissions::RequestType::kMultipleDownloads:
      return "multiple_downloads";
    case permissions::RequestType::kNotifications:
      return "notifications";
    case permissions::RequestType::kSensors:
      return "sensors";
    case permissions::RequestType::kPointerLock:
      return "pointer_lock";
    case permissions::RequestType::kRegisterProtocolHandler:
      return "register_protocol_handler";
    case permissions::RequestType::kStorageAccess:
      return "storage_access";
    case permissions::RequestType::kTopLevelStorageAccess:
      return "top_level_storage_access";
    case permissions::RequestType::kVrSession:
      return "vr_session";
    case permissions::RequestType::kWebAppInstallation:
      return "web_app_installation";
    case permissions::RequestType::kWindowManagement:
      return "window_management";
    case permissions::RequestType::kIdentityProvider:
      return "identity_provider";
  }

  return nullptr;
}

}  // namespace permissions
