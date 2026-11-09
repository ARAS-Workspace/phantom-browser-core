// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_

#include <memory>

#include "chrome/browser/ui/session_crashed_bubble.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class BrowserWindowInterface;

namespace views {
class BubbleDialogDelegate;
}

// SessionCrashedBubbleView shows a bubble allowing the user to restore the
// previous session.
class SessionCrashedBubbleView : public SessionCrashedBubble {
 public:
  // A helper class that listens to browser removal event.
  class BrowserRemovalObserver;

  // Creates and shows the session crashed bubble, with |skip_tab_checking|
  // indicating whether skip the tab checking. It will be called by
  // ShowIfNotOffTheRecordProfile. It takes ownership of |browser_observer|.
  static void Show(std::unique_ptr<BrowserRemovalObserver> browser_observer,
                   bool skip_tab_checking);

  static views::BubbleDialogDelegate* GetInstanceForTest();

 private:
  friend class SessionCrashedBubbleViewTest;

  // Internal show method also used by SessionCrashedBubbleViewTest.
  static views::BubbleDialogDelegate* ShowBubble(
      BrowserWindowInterface* browser);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_CRASHED_BUBBLE_VIEW_H_
