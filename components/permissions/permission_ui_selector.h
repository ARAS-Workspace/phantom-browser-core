// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_UI_SELECTOR_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_UI_SELECTOR_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/request_type.h"

namespace content {
class WebContents;
}

namespace permissions {

class PermissionRequest;

// The interface for implementations that decide if the quiet prompt UI should
// be used to display a permission |request|, and the reason for it.
//
// Implementations of interface are expected to have long-lived instances that
// can support multiple requests, but only one at a time.
class PermissionUiSelector {
 public:
  // LINT.IfChange(QuietUiReason)
  enum class QuietUiReason {
    kEnabledInPrefs,
    kServicePredictedVeryUnlikelyGrant,
    kOnDevicePredictedVeryUnlikelyGrant,
    kTriggeredDueToLackOfGesture,
  };
  // LINT.ThenChange(
  // //chrome/browser/ui/content_settings/content_setting_bubble_model.cc,
  // //components/permissions/permission_request_manager.cc)

  enum class GeolocationAccuracy {
    kUnspecified,
    kPrecise,
    kApproximate,
  };

  struct Decision {
    ~Decision();

    Decision(const Decision&);
    Decision& operator=(const Decision&);

    bool operator==(const Decision&) const;

    static Decision UseNormalUiAndShowNoWarning();

    static Decision UseNormalUi(GeolocationAccuracy geolocation_accuracy =
                                    GeolocationAccuracy::kUnspecified);

    static Decision UseQuietUi(QuietUiReason quiet_ui_reason);

    // The reason for showing the quiet UI, or `std::nullopt` if the normal UI
    // should be used.
    std::optional<QuietUiReason> quiet_ui_reason;

    // The preselected geolocation accuracy (for geolocation prompts).
    GeolocationAccuracy geolocation_accuracy;

   private:
    Decision(std::optional<QuietUiReason> quiet_ui_reason,
             GeolocationAccuracy geolocation_acuracy);
  };

  using DecisionMadeCallback = base::OnceCallback<void(const Decision&)>;

  virtual ~PermissionUiSelector() = default;

  // Determines the UI to use for the given |request|, and invokes |callback|
  // when done, either synchronously or asynchronously. The |callback| is
  // guaranteed never to be invoked after |this| goes out of scope. Only one
  // request is supported at a time.
  virtual void SelectUiToUse(content::WebContents* web_contents,
                             PermissionRequest* request,
                             DecisionMadeCallback callback) = 0;

  // Cancel the pending request, if any. After this, the |callback| is
  // guaranteed not to be invoked anymore, and another call to SelectUiToUse()
  // can be issued. Can be called when there is no pending request which will
  // simply be a no-op.
  virtual void Cancel() {}

  virtual bool IsPermissionRequestSupported(RequestType request_type) = 0;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_UI_SELECTOR_H_
