// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/default_browser_notification_observer.h"
#include "chrome/browser/default_browser/test_support/fake_default_browser_setter.h"
#include "chrome/browser/default_browser/test_support/fake_shell_delegate.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

namespace default_browser {

namespace {

}  // namespace

using DefaultBrowserManagerBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(DefaultBrowserManagerBrowserTest, OnAcceptedShowsToast) {
  DefaultBrowserController controller(
      std::make_unique<FakeDefaultBrowserSetter>(),
      DefaultBrowserEntrypointType::kSettingsPage);

  base::test::TestFuture<DefaultBrowserState> future;
  controller.OnAccepted(future.GetCallback());
  EXPECT_EQ(future.Get(), DefaultBrowserState::IS_DEFAULT);

  ToastController* toast_controller =
      browser()->GetFeatures().toast_controller();
  ASSERT_TRUE(toast_controller);
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(),
            ToastId::kDefaultBrowserUpdateSuccess);
}

}  // namespace default_browser
