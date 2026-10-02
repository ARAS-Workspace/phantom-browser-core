// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_SAFE_BROWSING_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_SAFE_BROWSING_UTIL_H_

#include <string>

#include "build/build_config.h"
#include "components/download/public/common/download_danger_type.h"

class Profile;

namespace download {
class DownloadItem;
}

// Utilities for determining how to display a download in the desktop UI based
// on Safe Browsing state and verdict.

// Returns whether the download item had a download protection verdict. If it
// did not, we should call it "unverified" rather than "suspicious".
bool WasSafeBrowsingVerdictObtained(const download::DownloadItem* item);

// For users with no Safe Browsing protections, we display a special warning.
// If this returns true, a filetype warning should say "unverified" instead of
// "suspicious".
bool ShouldShowWarningForNoSafeBrowsing(Profile* profile);

// Whether the user is capable of turning on Safe Browsing, e.g. it is not
// controlled by a policy.
bool CanUserTurnOnSafeBrowsing(Profile* profile);

// Utilities for recording actions taken on Safe Browsing-flagged downloads.

// Records UMA metrics for taking an action on the chrome://downloads warning
// bypass prompt. Logs to Download.DownloadDangerPrompt with the suffix, which
// can be "Proceed" or "Shown".
void RecordDownloadDangerPromptHistogram(
    const std::string& proceed_or_shown_suffix,
    const download::DownloadItem& item);

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_SAFE_BROWSING_UTIL_H_
