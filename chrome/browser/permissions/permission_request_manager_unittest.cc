// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_manager.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
#include "components/permissions/pref_names.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_manager/scoped_user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/permissions/android/android_permission_util.h"
#endif

class PermissionRequestManagerTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<std::pair<std::string, bool>> {
 public:
  PermissionRequestManagerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        params_request1_(permissions::RequestType::kGeolocation,
                         permissions::PermissionRequestGestureType::GESTURE),
        params_request2_(permissions::RequestType::kMultipleDownloads,
                         permissions::PermissionRequestGestureType::NO_GESTURE),
        params_request_mic_(
            permissions::RequestType::kMicStream,
            permissions::PermissionRequestGestureType::NO_GESTURE),
        params_request_camera_(
            permissions::RequestType::kCameraStream,
            permissions::PermissionRequestGestureType::NO_GESTURE) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL(permissions::MockPermissionRequest::kDefaultOrigin));

    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    manager_->set_enabled_app_level_notification_permission_for_testing(true);
    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager_);
  }

  void TearDown() override {
    prompt_factory_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void Accept(const PromptOptions& prompt_options = std::monostate()) {
    manager_->Accept(prompt_options);
    base::RunLoop().RunUntilIdle();
  }

  void AcceptThisTime(const PromptOptions& prompt_options = std::monostate()) {
    manager_->AcceptThisTime(prompt_options);
    base::RunLoop().RunUntilIdle();
  }

  void Deny() {
    manager_->Deny(/*prompt_options=*/std::monostate());
    base::RunLoop().RunUntilIdle();
  }

  void Closing() {
    manager_->Dismiss(/*prompt_options=*/std::monostate());
    base::RunLoop().RunUntilIdle();
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInPrimaryMainFrame();
    base::RunLoop().RunUntilIdle();
  }

  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& details) {
    manager_->NavigationEntryCommitted(details);
  }

 protected:
  std::unique_ptr<permissions::MockPermissionRequest> CreateRequest(
      std::pair<permissions::RequestType,
                permissions::PermissionRequestGestureType> request_params) {
    return std::make_unique<permissions::MockPermissionRequest>(
        request_params.first, request_params.second);
  }

  std::pair<permissions::RequestType, permissions::PermissionRequestGestureType>
      params_request1_;
  std::pair<permissions::RequestType, permissions::PermissionRequestGestureType>
      params_request2_;
  std::pair<permissions::RequestType, permissions::PermissionRequestGestureType>
      params_request_mic_;
  std::pair<permissions::RequestType, permissions::PermissionRequestGestureType>
      params_request_camera_;
  raw_ptr<permissions::PermissionRequestManager, DanglingUntriaged> manager_;
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
};

TEST_F(PermissionRequestManagerTest, UMAForSimpleAcceptedGestureBubble) {
  base::HistogramTester histograms;

  permissions::RequestTypeForUma geolocation_request_type =
      base::FeatureList::IsEnabled(
          content_settings::features::kApproximateGeolocationPermission)
          ? permissions::RequestTypeForUma::
                PERMISSION_GEOLOCATION_APPROXIMATE_OR_PRECISE
          : permissions::RequestTypeForUma::PERMISSION_GEOLOCATION;
  std::string_view geolocation_prompt_name =
      base::FeatureList::IsEnabled(
          content_settings::features::kApproximateGeolocationPermission)
          ? "GeolocationApproximateOrPrecise"
          : "Geolocation";

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request1_));
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample32>(geolocation_request_type), 1);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture,
      static_cast<base::HistogramBase::Sample32>(geolocation_request_type), 1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);

  Accept(base::FeatureList::IsEnabled(
             content_settings::features::kApproximateGeolocationPermission)
             ? PromptOptions(GeolocationPromptOptions{
                   .selected_accuracy = GeolocationAccuracy::kPrecise})
             : std::monostate());
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample32>(geolocation_request_type), 1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied, 0);

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAcceptedGesture,
      static_cast<base::HistogramBase::Sample32>(geolocation_request_type), 1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptAcceptedNoGesture, 0);
}

TEST_F(PermissionRequestManagerTest, UMAForSimpleDeniedNoGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request2_));
  WaitForBubbleToBeShown();

  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture, 0);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::DOWNLOAD),
      1);
  // No need to test the other UMA for showing prompts again, they were tested
  // in UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptAccepted, 0);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::DOWNLOAD),
      1);

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDeniedNoGesture,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::DOWNLOAD),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptDeniedGesture, 0);
}

TEST_F(PermissionRequestManagerTest, UMAForMergedAcceptedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request_mic_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request_camera_));
  WaitForBubbleToBeShown();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture, 0);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);

  Accept();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE),
      1);
}

TEST_F(PermissionRequestManagerTest, UMAForMergedDeniedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request_mic_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request_camera_));
  WaitForBubbleToBeShown();
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForMergedAcceptedBubble.

  Deny();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE),
      1);
}

TEST_F(PermissionRequestManagerTest, TestEmbargoForEmbeddedPermissionRequest) {
#if BUILDFLAG(IS_ANDROID)
  base::AutoReset<bool> enable_android_permissions =
      permissions::EnableAllAndroidPermissionsForTesting();
#endif
  system_permission_settings::ScopedSettingsForTesting scoped_system_permission(
      ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/false);

  GURL url(permissions::MockPermissionRequest::kDefaultOrigin);
  permissions::RequestType request_type =
      permissions::RequestType::kCameraStream;
  permissions::PermissionDecisionAutoBlocker* autoblocker =
      permissions::PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          browser_context());

  // Do not count permission element requests towards embargo
  {
    auto request = std::make_unique<permissions::MockPermissionRequest>(
        GURL(permissions::MockPermissionRequest::kDefaultOrigin), request_type,
        /* embedded_permission_element_initiated= */ true);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();
    Closing();

    EXPECT_EQ(
        autoblocker->GetDismissCount(
            url, permissions::RequestTypeToContentSettingsType(request_type)
                     .value()),
        0);
  }

  // Count normal permission towards embargo (used in next step)
  {
    auto request = std::make_unique<permissions::MockPermissionRequest>(
        GURL(permissions::MockPermissionRequest::kDefaultOrigin), request_type,
        /* embedded_permission_element_initiated= */ false);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();
    Closing();

    EXPECT_EQ(
        autoblocker->GetDismissCount(
            url, permissions::RequestTypeToContentSettingsType(request_type)
                     .value()),
        1);
  }

  // Reset embargo counter when accepting this time and using permission element
  {
    auto request = std::make_unique<permissions::MockPermissionRequest>(
        GURL(permissions::MockPermissionRequest::kDefaultOrigin), request_type,
        /* embedded_permission_element_initiated= */ true);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();
    AcceptThisTime();

    EXPECT_EQ(
        autoblocker->GetDismissCount(
            url, permissions::RequestTypeToContentSettingsType(request_type)
                     .value()),
        0);
  }
}

class ChromePermissionRequestManagerAdaptiveQuietUiActivationTest
    : public PermissionRequestManagerTest {
 public:
  ChromePermissionRequestManagerAdaptiveQuietUiActivationTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kQuietNotificationPrompts,
          {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
            "true"}}}},
        {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};
