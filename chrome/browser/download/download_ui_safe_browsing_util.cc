// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_safe_browsing_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"

bool WasSafeBrowsingVerdictObtained(const download::DownloadItem* item) {
  return false;
}

bool ShouldShowWarningForNoSafeBrowsing(Profile* profile) {
  return true;
}

bool CanUserTurnOnSafeBrowsing(Profile* profile) {
  return false;
}

void RecordDownloadDangerPromptHistogram(
    const std::string& proceed_or_shown_suffix,
    const download::DownloadItem& item) {
}

