// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_alert.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "url/gurl.h"

namespace {
class TabAlertControllerObserver
    : public ui::test::StateObserver<std::optional<tabs::TabAlert>> {
 public:
  TabAlertControllerObserver(BrowserWindowInterface* browser, int tab_index) {
    callback_subscription_ =
        tabs::TabAlertController::From(
            browser->tab_strip_model()->GetTabAtIndex(tab_index))
            ->AddAlertToShowChangedCallback(base::BindRepeating(
                &TabAlertControllerObserver::OnAlertToShowChanged,
                base::Unretained(this)));
  }

  void OnAlertToShowChanged(std::optional<tabs::TabAlert> alert) {
    OnStateObserverStateChanged(alert);
  }

  ~TabAlertControllerObserver() override = default;

 private:
  base::CallbackListSubscription callback_subscription_;
};

}  // namespace
