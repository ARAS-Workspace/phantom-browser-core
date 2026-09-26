// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request.h"

#include <string>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "services/device/public/cpp/device_features.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/strings/grit/ui_strings.h"

namespace permissions {

namespace {

bool AreGenericSensorExtraClassesEnabled() {
  return base::FeatureList::IsEnabled(::features::kGenericSensorExtraClasses);
}

}  // namespace

PermissionRequest::PermissionRequest(
    std::unique_ptr<PermissionRequestData> request_data,
    PermissionDecidedCallback permission_decided_callback,
    base::OnceClosure request_finished_callback,
    bool uses_automatic_embargo)
    : data_(std::move(request_data)),
      permission_decided_callback_(std::move(permission_decided_callback)),
      request_finished_callback_(std::move(request_finished_callback)),
      uses_automatic_embargo_(uses_automatic_embargo) {}

PermissionRequest::~PermissionRequest() {
  std::move(request_finished_callback_).Run();
}

RequestType PermissionRequest::request_type() const {
  CHECK(data_->request_type);
  return data_->request_type.value();
}

bool PermissionRequest::IsDuplicateOf(PermissionRequest* other_request) const {
  return request_type() == other_request->request_type() &&
         requesting_origin() == other_request->requesting_origin();
}

base::SafeRef<PermissionRequest> PermissionRequest::GetSafeRef() {
  return weak_factory_.GetSafeRef();
}

bool PermissionRequest::IsEmbeddedPermissionElementInitiated() const {
  return data_->IsEmbeddedPermissionElementInitiated();
}

bool PermissionRequest::IsGeolocationElementInitiated() const {
  return data_->IsGeolocationElementInitiated();
}

bool PermissionRequest::IsEligibleForHeuristicAutoGrant() const {
  return data_->IsEligibleForHeuristicAutoGrant();
}

std::optional<gfx::Rect> PermissionRequest::GetAnchorElementPosition() const {
  return data_->GetAnchorElementPosition();
}

bool PermissionRequest::IsConfirmationChipSupported() {
  return permissions::IsConfirmationChipSupported(request_type());
}

IconId PermissionRequest::GetIconForChip() {
  return permissions::GetIconId(request_type());
}

IconId PermissionRequest::GetBlockedIconForChip() {
  return permissions::GetBlockedIconId(request_type());
}

std::optional<std::u16string> PermissionRequest::GetRequestChipText(
    ChipTextType type) const {
  static base::NoDestructor<std::map<RequestType, std::vector<int>>> kMessageIds(
      {{RequestType::kArSession,
        {IDS_AR_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_AR_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_AR_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_AR_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kCameraStream,
        {IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_CAMERA_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_CAMERA_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_CAMERA_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kCapturedSurfaceControl,
        {IDS_CAPTURED_SURFACE_CONTROL_PERMISSION_CHIP,
         IDS_CAPTURED_SURFACE_CONTROL_PERMISSION_BLOCKED_CHIP,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_CAPTURED_SURFACE_CONTROL_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_CAPTURED_SURFACE_CONTROL_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kClipboard,
        {IDS_CLIPBOARD_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kGeolocation,
        {IDS_GEOLOCATION_PERMISSION_CHIP,
         IDS_GEOLOCATION_PERMISSION_BLOCKED_CHIP,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_GEOLOCATION_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_GEOLOCATION_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kHandTracking,
        {IDS_HAND_TRACKING_PERMISSION_CHIP,
         IDS_HAND_TRACKING_PERMISSION_BLOCKED_CHIP,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_HAND_TRACKING_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_HAND_TRACKING_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_HAND_TRACKING_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kIdleDetection,
        {IDS_IDLE_DETECTION_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kKeyboardLock,
        {IDS_KEYBOARD_LOCK_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_KEYBOARD_LOCK_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_KEYBOARD_LOCK_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kMicStream,
        {IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_MICROPHONE_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_MICROPHONE_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_MICROPHONE_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kMidiSysex,
        {IDS_MIDI_SYSEX_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kNotifications,
        {IDS_NOTIFICATION_PERMISSIONS_CHIP,
         IDS_NOTIFICATION_PERMISSIONS_BLOCKED_CHIP,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION, -1,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_NOTIFICATION_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         -1,
         IDS_PERMISSIONS_NOTIFICATION_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kPointerLock,
        {IDS_POINTER_LOCK_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_POINTER_LOCK_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_POINTER_LOCK_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kSensors,
        {AreGenericSensorExtraClassesEnabled()
             ? IDS_MOTION_AND_LIGHT_SENSORS_PERMISSION_CHIP
             : IDS_MOTION_SENSORS_PERMISSION_CHIP,
         -1 /* QUIET_REQUEST not supported */,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         AreGenericSensorExtraClassesEnabled()
             ? IDS_PERMISSIONS_MOTION_AND_LIGHT_SENSORS_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT
             : IDS_PERMISSIONS_MOTION_SENSORS_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         AreGenericSensorExtraClassesEnabled()
             ? IDS_PERMISSIONS_MOTION_AND_LIGHT_SENSORS_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT
             : IDS_PERMISSIONS_MOTION_SENSORS_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         AreGenericSensorExtraClassesEnabled()
             ? IDS_PERMISSIONS_MOTION_AND_LIGHT_SENSORS_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT
             : IDS_PERMISSIONS_MOTION_SENSORS_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kStorageAccess,
        {IDS_SAA_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION, -1,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_SAA_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT, -1,
         IDS_PERMISSIONS_SAA_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kVrSession,
        {IDS_VR_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_VR_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_VR_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_VR_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kWebAppInstallation,
        {IDS_WEB_APP_INSTALLATION_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_WEB_INSTALL_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_WEB_INSTALL_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}}});

  auto messages = kMessageIds->find(request_type());
  if (messages != kMessageIds->end() && messages->second[type] != -1) {
    return l10n_util::GetStringUTF16(messages->second[type]);
  }

  return std::nullopt;
}

std::u16string PermissionRequest::GetMessageTextFragment() const {
  int message_id = 0;
  switch (request_type()) {
    case RequestType::kArSession:
      message_id = IDS_AR_PERMISSION_FRAGMENT;
      break;
    case RequestType::kCameraPanTiltZoom:
      message_id = IDS_MEDIA_CAPTURE_CAMERA_PAN_TILT_ZOOM_PERMISSION_FRAGMENT;
      break;
    case RequestType::kCameraStream:
      message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT;
      break;
    case RequestType::kCapturedSurfaceControl:
      message_id = IDS_CAPTURED_SURFACE_CONTROL_PERMISSION_FRAGMENT;
      break;
    case RequestType::kClipboard:
      message_id = IDS_CLIPBOARD_PERMISSION_FRAGMENT;
      break;
    case RequestType::kDiskQuota:
      message_id = IDS_REQUEST_QUOTA_PERMISSION_FRAGMENT;
      break;
    case RequestType::kFileSystemAccess:
      message_id = IDS_SITE_SETTINGS_TYPE_FILE_SYSTEM_ACCESS_WRITE;
      break;
    case RequestType::kGeolocation:
      message_id = IDS_GEOLOCATION_INFOBAR_PERMISSION_FRAGMENT;
      break;
    case RequestType::kHandTracking:
      message_id = IDS_HAND_TRACKING_PERMISSION_FRAGMENT;
      break;
    case RequestType::kIdleDetection:
      message_id = IDS_IDLE_DETECTION_PERMISSION_FRAGMENT;
      break;
    case RequestType::kKeyboardLock:
      message_id = IDS_KEYBOARD_LOCK_PERMISSIONS_FRAGMENT;
      break;
    case RequestType::kLocalFonts:
      message_id = IDS_FONT_ACCESS_PERMISSION_FRAGMENT;
      break;
    case RequestType::kLocalNetwork:
      message_id = IDS_LOCAL_NETWORK_PERMISSION_FRAGMENT;
      break;
    case RequestType::kLoopbackNetwork:
      message_id = IDS_LOOPBACK_NETWORK_PERMISSION_FRAGMENT;
      break;
    case RequestType::kMicStream:
      message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT;
      break;
    case RequestType::kMidiSysex:
      message_id = IDS_MIDI_SYSEX_PERMISSION_FRAGMENT;
      break;
    case RequestType::kMultipleDownloads:
      message_id = IDS_MULTI_DOWNLOAD_PERMISSION_FRAGMENT;
      break;
    case RequestType::kNotifications:
      message_id = IDS_NOTIFICATION_PERMISSIONS_FRAGMENT;
      break;
    case RequestType::kSensors:
      message_id = AreGenericSensorExtraClassesEnabled()
                       ? IDS_MOTION_AND_LIGHT_SENSORS_PERMISSION_FRAGMENT
                       : IDS_MOTION_SENSORS_PERMISSION_FRAGMENT;
      break;
    case RequestType::kPointerLock:
      message_id = IDS_POINTER_LOCK_PERMISSIONS_FRAGMENT;
      break;
    case RequestType::kRegisterProtocolHandler:
      // Handled by an override in `RegisterProtocolHandlerPermissionRequest`.
      NOTREACHED();
    case RequestType::kStorageAccess:
    case RequestType::kTopLevelStorageAccess:
      message_id = IDS_STORAGE_ACCESS_PERMISSION_FRAGMENT;
      break;
    case RequestType::kVrSession:
      message_id = IDS_VR_PERMISSION_FRAGMENT;
      break;
    case RequestType::kWebAppInstallation:
      message_id = IDS_WEB_APP_INSTALLATION_PERMISSION_FRAGMENT;
      break;
    case RequestType::kWindowManagement:
      message_id = IDS_WINDOW_MANAGEMENT_PERMISSION_FRAGMENT;
      break;
    case RequestType::kIdentityProvider:
      message_id = IDS_IDENTITY_PROVIDER_PERMISSION_FRAGMENT;
      break;
  }
  DCHECK_NE(0, message_id);
  return l10n_util::GetStringUTF16(message_id);
}

std::optional<std::u16string> PermissionRequest::GetAllowAlwaysText() const {
  return std::nullopt;
}

std::optional<std::u16string> PermissionRequest::GetBlockText() const {
  return std::nullopt;
}

bool PermissionRequest::ShouldUseTwoOriginPrompt() const {
  return request_type() == RequestType::kStorageAccess;
}

std::optional<GeolocationPromptType>
PermissionRequest::GetGeolocationPromptType() const {
  return data_->geolocation_prompt_type;
}

void PermissionRequest::PermissionGranted(const PromptOptions& prompt_options,
                                          bool is_one_time) {
  std::move(permission_decided_callback_)
      .Run(PermissionPromptDecision{.overall_decision =
                                        is_one_time
                                            ? PermissionDecision::kAllowThisTime
                                            : PermissionDecision::kAllow,
                                    .prompt_options = prompt_options,
                                    .is_final = true},
           /*request_data=*/*data_);
}

void PermissionRequest::PermissionDenied() {
  std::move(permission_decided_callback_)
      .Run(PermissionPromptDecision{.overall_decision =
                                        PermissionDecision::kDeny,
                                    .prompt_options = std::monostate(),
                                    .is_final = true},
           /*request_data=*/*data_);
}

void PermissionRequest::Cancelled(bool is_final_decision) {
  if (permission_decided_callback_) {
    permission_decided_callback_.Run(
        PermissionPromptDecision{.overall_decision = PermissionDecision::kNone,
                                 .prompt_options = std::monostate(),
                                 .is_final = is_final_decision},
        /*request_data=*/*data_);
  }
}

PermissionRequestGestureType PermissionRequest::GetGestureType() const {
  return PermissionUtil::GetGestureType(data_->user_gesture);
}

const std::vector<std::string>&
PermissionRequest::GetRequestedAudioCaptureDeviceIds() const {
  return data_->requested_audio_capture_device_ids;
}

const std::vector<std::string>&
PermissionRequest::GetRequestedVideoCaptureDeviceIds() const {
  return data_->requested_video_capture_device_ids;
}

ContentSettingsType PermissionRequest::GetContentSettingsType() const {
  auto type = RequestTypeToContentSettingsType(request_type());
  if (type.has_value()) {
    return type.value();
  }
  return ContentSettingsType::DEFAULT;
}

std::u16string PermissionRequest::GetPermissionNameTextFragment() const {
  int message_id = 0;
  switch (request_type()) {
    case RequestType::kCameraStream:
      message_id = IDS_CAMERA_PERMISSION_NAME_FRAGMENT;
      break;
    case RequestType::kGeolocation:
      message_id = IDS_GEOLOCATION_NAME_FRAGMENT;
      break;
    case RequestType::kMicStream:
      message_id = IDS_MICROPHONE_PERMISSION_NAME_FRAGMENT;
      break;
    default:
      NOTREACHED();
  }
  DCHECK_NE(0, message_id);
  return l10n_util::GetStringUTF16(message_id);
}

void PermissionRequest::SetEmbeddedPermissionElementInitiatedForTesting(
    bool embedded_permission_element_initiated) {
  if (embedded_permission_element_initiated) {
    data_->embedded_permission_request_descriptor =
        blink::mojom::EmbeddedPermissionRequestDescriptor::New();
  }
}

bool PermissionRequest::IsSourceSubscribedToPermissionChangeEvent(
    content::PermissionController* controller) const {
  DCHECK(controller);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(get_requesting_frame_id());

  if (rfh == nullptr) {
    return false;
  }

  blink::PermissionType permission_type =
      permissions::PermissionUtil::ContentSettingsTypeToPermissionType(
          GetContentSettingsType());

  return controller->IsSubscribedToPermissionChangeEvent(permission_type, rfh);
}

}  // namespace permissions
