// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/enums_to_string.h"

#include "base/containers/fixed_flat_map.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/request_type.h"

namespace test {
std::string_view ToString(
    permissions::PermissionUiSelector::QuietUiReason ui_reason) {
  using QuietUiReason = ::permissions::PermissionUiSelector::QuietUiReason;
  static constexpr auto map =
      base::MakeFixedFlatMap<QuietUiReason, std::string_view>(
          {{QuietUiReason::kEnabledInPrefs, "EnabledInPrefs"},
           {QuietUiReason::kTriggeredByCrowdDeny, "TriggeredByCrowdDeny"},
           {QuietUiReason::kServicePredictedVeryUnlikelyGrant,
            "ServicePredictedVeryUnlikelyGrant"},
           {QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant,
            "OnDevicePredictedVeryUnlikelyGrant"},
           {QuietUiReason::kTriggeredDueToAbusiveRequests,
            "TriggeredDueToAbusiveRequests"},
           {QuietUiReason::kTriggeredDueToAbusiveContent,
            "TriggeredDueToAbusiveContent"},
           {QuietUiReason::kTriggeredDueToDisruptiveBehavior,
            "TriggeredDueToDisruptiveBehavior"}});

  auto it = map.find(ui_reason);
  return (it == map.end()) ? "Unknown" : it->second;
}

std::string_view ToString(permissions::RequestType request_type) {
  using RequestType = ::permissions::RequestType;

  static constexpr auto map =
      base::MakeFixedFlatMap<RequestType, std::string_view>({
          {RequestType::kArSession, "ArSession"},
#if !BUILDFLAG(IS_ANDROID)
          {RequestType::kCameraPanTiltZoom, "CameraPanTiltZoom"},
#endif  // !BUILDFLAG(IS_ANDROID)
          {RequestType::kCameraStream, "CameraStream"},
#if !BUILDFLAG(IS_ANDROID)
          {RequestType::kCapturedSurfaceControl, "CapturedSurfaceControl"},
#endif  // !BUILDFLAG(IS_ANDROID)
          {RequestType::kClipboard, "Clipboard"},
          {RequestType::kTopLevelStorageAccess, "TopLevelStorageAccess"},
          {RequestType::kDiskQuota, "DiskQuota"},
          {RequestType::kFileSystemAccess, "FileSystemAccess"},
          {RequestType::kGeolocation, "Geolocation"},
          {RequestType::kHandTracking, "HandTracking"},
          {RequestType::kIdentityProvider, "IdentityProvider"},
          {RequestType::kIdleDetection, "IdleDetection"},
#if !BUILDFLAG(IS_ANDROID)
          {RequestType::kLocalFonts, "LocalFonts"},
#endif  // !BUILDFLAG(IS_ANDROID)
          {RequestType::kMicStream, "MicStream"},
          {RequestType::kMidiSysex, "MidiSysex"},
          {RequestType::kMultipleDownloads, "MultipleDownloads"},
#if BUILDFLAG(IS_ANDROID)
          {RequestType::kNfcDevice, "NfcDevice"},
#endif  // BUILDFLAG(IS_ANDROID)
          {RequestType::kNotifications, "Notifications"},
#if !BUILDFLAG(IS_ANDROID)
          {RequestType::kKeyboardLock, "KeyboardLock"},
          {RequestType::kPointerLock, "PointerLock"},
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_ANDROID)
          {RequestType::kProtectedMediaIdentifier, "ProtectedMediaIdentifier"},
#endif
#if !BUILDFLAG(IS_ANDROID)
          {RequestType::kRegisterProtocolHandler, "RegisterProtocolHandler"},
#endif  // !BUILDFLAG(IS_ANDROID)
          {RequestType::kStorageAccess, "StorageAccess"},
          {RequestType::kVrSession, "VrSession"},
#if !BUILDFLAG(IS_ANDROID)
          {RequestType::kWebAppInstallation, "WebAppInstallation"},
#endif  // !BUILDFLAG(IS_ANDROID)
          {RequestType::kWindowManagement, "WindowManagement"},
      });

  auto it = map.find(request_type);
  return (it == map.end()) ? "Unknown" : it->second;
}

}  // namespace test
