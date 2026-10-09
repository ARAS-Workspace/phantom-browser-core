// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_
#define CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_

namespace web_app {
namespace startup {

// Various ways web apps can open on Chrome launch.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OpenMode {
  // Launched app by any method other than through the command-line or from a
  // platform shortcut.
  kInWindowOther = 0,
  kInTab = 1,            // Launched as an installed web app in a browser tab.
  kUnknown = 2,          // The requested web app was not installed.
  kInWindowByUrl = 3,    // Launched the app by url with --app switch.
  kInWindowByAppId = 4,  // Launched app by id with --app-id switch.
  kMaxValue = kInWindowByAppId,
};

}  // namespace startup
}  // namespace web_app

#endif  // CHROME_BROWSER_UI_STARTUP_WEB_APP_STARTUP_UTILS_H_
