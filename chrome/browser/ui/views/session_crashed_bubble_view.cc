// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/buildflags.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

views::BubbleDialogDelegate* g_instance_for_test = nullptr;

enum SessionCrashedBubbleHistogramValue {
  SESSION_CRASHED_BUBBLE_SHOWN,
  SESSION_CRASHED_BUBBLE_ERROR,
  SESSION_CRASHED_BUBBLE_RESTORED,
  SESSION_CRASHED_BUBBLE_ALREADY_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_HELP,
  SESSION_CRASHED_BUBBLE_IGNORED,
  SESSION_CRASHED_BUBBLE_OPTIN_BAR_SHOWN,
  SESSION_CRASHED_BUBBLE_STARTUP_PAGES,
  SESSION_CRASHED_BUBBLE_MAX,
};

void RecordBubbleHistogramValue(SessionCrashedBubbleHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION("Session.SessionCrashed.Bubble", value,
                            SESSION_CRASHED_BUBBLE_MAX);
}

class SessionCrashedBubbleDelegate : public ui::DialogModelDelegate {
 public:
  explicit SessionCrashedBubbleDelegate(Profile* profile) {
    if (ExitTypeService* exit_type_service =
            ExitTypeService::GetInstanceForProfile(profile)) {
      crashed_lock_ = exit_type_service->CreateCrashedLock();
    }

  }

  ~SessionCrashedBubbleDelegate() override { g_instance_for_test = nullptr; }


  void OpenStartupPages(BrowserWindowInterface* browser) {
    ignored_ = false;
    dialog_model()->host()->Close();

    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_STARTUP_PAGES);
    // Opening tabs has side effects, so it's preferable to do it after the
    // bubble was closed.
    SessionRestore::OpenStartupPagesAfterCrash(browser);
  }

  void OnWindowClosing() {
    if (ignored_) {
      RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_IGNORED);
    }
  }

  void RestorePreviousSession(BrowserWindowInterface* browser) {
    ignored_ = false;
    // The call to Close() deletes this. Grab the lock so that session restore
    // is triggered before the lock is destroyed, otherwise ExitTypeService
    // won't wait for restore to complete.
    std::unique_ptr<ExitTypeService::CrashedLock> lock =
        std::move(crashed_lock_);
    dialog_model()->host()->Close();

    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_RESTORED);
    // Restoring tabs has side effects, so it's preferable to do it after the
    // bubble was closed.
    SessionRestore::RestoreSessionAfterCrash(browser);
  }

 private:
  bool ignored_ = true;
  std::unique_ptr<ExitTypeService::CrashedLock> crashed_lock_;
};

}  // namespace

// A helper class that listens to browser removal event.
class SessionCrashedBubbleView::BrowserRemovalObserver
    : public BrowserCollectionObserver {
 public:
  explicit BrowserRemovalObserver(BrowserWindowInterface* browser)
      : browser_(browser) {
    DCHECK(browser_);
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
  }

  BrowserRemovalObserver(const BrowserRemovalObserver&) = delete;
  BrowserRemovalObserver& operator=(const BrowserRemovalObserver&) = delete;

  ~BrowserRemovalObserver() override = default;

  // BrowserCollectionObserver:
  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    if (browser == browser_) {
      browser_ = nullptr;
    }
  }

  BrowserWindowInterface* browser() const { return browser_; }

 private:
  raw_ptr<BrowserWindowInterface> browser_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

// static
void SessionCrashedBubble::ShowIfNotOffTheRecordProfile(
    BrowserWindowInterface* browser,
    bool skip_tab_checking) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (browser->GetProfile()->IsOffTheRecord()) {
    return;
  }

  // Observes possible browser removal before Show is called.
  auto browser_observer =
      std::make_unique<SessionCrashedBubbleView::BrowserRemovalObserver>(
          browser);

  SessionCrashedBubbleView::Show(std::move(browser_observer),
                                 skip_tab_checking);
}

// static
void SessionCrashedBubbleView::Show(
    std::unique_ptr<BrowserRemovalObserver> browser_observer,
    bool skip_tab_checking) {
  BrowserWindowInterface* browser = browser_observer->browser();

  if (browser && (skip_tab_checking ||
                  browser->GetTabStripModel()->GetActiveWebContents())) {
    ShowBubble(browser);
    return;
  }

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ERROR);
}

// static
views::BubbleDialogDelegate* SessionCrashedBubbleView::GetInstanceForTest() {
  return g_instance_for_test;
}

views::BubbleDialogDelegate* SessionCrashedBubbleView::ShowBubble(
    BrowserWindowInterface* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  // TODO(webium): WebUI browser does not use BrowserView. Make an WebUI anchor
  // for the bubble.
  if (!browser_view) {
    return nullptr;
  }

  auto* control = browser_view->toolbar_button_provider()->GetAppMenuControl();
  views::BubbleAnchor anchor =
      control ? control->GetAnchor() : views::BubbleAnchor();

  auto bubble_delegate_unique =
      std::make_unique<SessionCrashedBubbleDelegate>(browser->GetProfile());
  SessionCrashedBubbleDelegate* bubble_delegate = bubble_delegate_unique.get();

  ui::DialogModel::Builder dialog_builder(std::move(bubble_delegate_unique));
  dialog_builder
      .SetTitle(l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_BUBBLE_TITLE))
      .DisableCloseOnDeactivate()
      .SetIsAlertDialog()
      .SetDialogDestroyingCallback(
          base::BindOnce(&SessionCrashedBubbleDelegate::OnWindowClosing,
                         base::Unretained(bubble_delegate)))
      .AddParagraph(ui::DialogModelLabel(IDS_SESSION_CRASHED_VIEW_MESSAGE));

  const SessionStartupPref session_startup_pref =
      SessionStartupPref::GetStartupPref(browser->GetProfile());

  if (session_startup_pref.ShouldOpenUrls() &&
      !session_startup_pref.urls.empty()) {
    dialog_builder.AddCancelButton(
        base::BindOnce(&SessionCrashedBubbleDelegate::OpenStartupPages,
                       base::Unretained(bubble_delegate), browser));
  }

  dialog_builder.AddOkButton(
      base::BindOnce(&SessionCrashedBubbleDelegate::RestorePreviousSession,
                     base::Unretained(bubble_delegate), browser),
      ui::DialogModel::Button::Params().SetLabel(
          l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON)));

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_builder.Build(), anchor, views::BubbleBorder::TOP_RIGHT);

  views::BubbleDialogDelegate* bubble_ptr = bubble.get();
  g_instance_for_test = bubble_ptr;
  views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(bubble), views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
      ->Show();

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_SHOWN);
  return bubble_ptr;
}
