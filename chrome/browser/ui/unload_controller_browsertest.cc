// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/unload_controller.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/prevent_close_test_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace {
constexpr char kCalculatorAppUrl[] = "https://calculator.apps.chrome/";

constexpr char kPreventCloseEnabledForCalculator[] = R"([
  {
    "manifest_id": "https://calculator.apps.chrome/",
    "run_on_os_login": "run_windowed",
    "prevent_close_after_run_on_os_login": true
  }
])";

constexpr char kCalculatorForceInstalled[] = R"([
  {
    "url": "https://calculator.apps.chrome/",
    "default_launch_container": "window"
  }
])";

constexpr bool kShouldPreventClose = false;

}  // namespace

class UnloadControllerPreventCloseTest
    : public VerticalTabsBrowserTestMixin<PreventCloseTestBase>,
      public testing::WithParamInterface<TabStripOrientation> {
 public:
  UnloadControllerPreventCloseTest() = default;
  ~UnloadControllerPreventCloseTest() override = default;

  TabStripOrientation orientation() const { return GetParam(); }
  bool is_horizontal() const {
    return orientation() == TabStripOrientation::kHorizontal;
  }

  void SetUpOnMainThread() override {
    VerticalTabsBrowserTestMixin<PreventCloseTestBase>::SetUpOnMainThread();
    if (is_horizontal()) {
      ExitVerticalTabsMode();
    }
  }
};

IN_PROC_BROWSER_TEST_P(UnloadControllerPreventCloseTest,
                       PreventCloseEnforcedByPolicy) {
  const absl::Cleanup policy_cleanup = [this] {
    SetPolicies(/*web_app_settings=*/"[]", /*web_app_install_force_list=*/"[]");
  };

  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(web_app::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);

  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/true);
  ASSERT_TRUE(browser);

  UnloadController* unload_controller = UnloadController::From(browser);
  EXPECT_EQ(kShouldPreventClose
                ? BrowserWindowInterface::ClosingStatus::kDeniedByPolicy
                : BrowserWindowInterface::ClosingStatus::kPermitted,
            unload_controller->GetBrowserClosingStatus());
}

#define MAYBE_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable \
  PreventCloseEnforcedByPolicyTabbedAppShallBeClosable
IN_PROC_BROWSER_TEST_P(
    UnloadControllerPreventCloseTest,
    MAYBE_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable) {
  const absl::Cleanup policy_cleanup = [this] {
    SetPolicies(/*web_app_settings=*/"[]", /*web_app_install_force_list=*/"[]");
  };

  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(web_app::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);

  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/false);
  ASSERT_TRUE(browser);

  UnloadController* unload_controller = UnloadController::From(browser);
  EXPECT_EQ(BrowserWindowInterface::ClosingStatus::kPermitted,
            unload_controller->GetBrowserClosingStatus());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UnloadControllerPreventCloseTest,
    testing::Values(TabStripOrientation::kVertical,
                    TabStripOrientation::kHorizontal),
    [](const testing::TestParamInfo<TabStripOrientation>& info) {
      switch (info.param) {
        case TabStripOrientation::kVertical:
          return "Vertical";
        case TabStripOrientation::kHorizontal:
          return "Horizontal";
      }
    });
