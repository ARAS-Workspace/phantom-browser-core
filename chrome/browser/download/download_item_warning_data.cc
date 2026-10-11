// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_warning_data.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/download/public/common/download_item.h"

using download::DownloadItem;
using WarningSurface = DownloadItemWarningData::WarningSurface;
using WarningAction = DownloadItemWarningData::WarningAction;

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AddWarningActionEventOutcome {
  // `download` was nullptr. This should never happen.
  NOT_ADDED_MISSING_DOWNLOAD = 0,
  // The first warning shown event is already logged so it is not logged this
  // time.
  NOT_ADDED_WARNING_SHOWN_ALREADY_LOGGED = 1,
  // The warning action event is not added because the first warning shown event
  // was not logged before.
  NOT_ADDED_MISSING_FIRST_WARNING = 2,
  // The warning action event is not added because it exceeds the max length.
  NOT_ADDED_EXCEED_MAX_LENGTH = 3,
  // The first warning shown event is successfully added.
  ADDED_WARNING_FIRST_SHOWN = 4,
  // The warning action event is successfully added.
  ADDED_WARNING_ACTION = 5,
  // The warning action event is not added because the download is not
  // dangerous.
  NOT_ADDED_DOWNLOAD_NOT_DANGEROUS = 6,
  kMaxValue = NOT_ADDED_DOWNLOAD_NOT_DANGEROUS
};

void RecordAddWarningActionEventOutcome(AddWarningActionEventOutcome outcome) {
  base::UmaHistogramEnumeration(
      "Download.WarningData.AddWarningActionEventOutcome2", outcome);
}

void RecordWarningActionAdded(WarningAction action) {
  base::UmaHistogramEnumeration("Download.WarningData.ActionAdded", action);
}

}  // namespace

// static
const char DownloadItemWarningData::kKey[] = "DownloadItemWarningData key";

// static
DownloadItemWarningData* DownloadItemWarningData::GetOrCreate(
    DownloadItem* download) {
  DownloadItemWarningData* data =
      static_cast<DownloadItemWarningData*>(download->GetUserData(kKey));
  if (!data) {
    data = new DownloadItemWarningData();
    download->SetUserData(kKey, base::WrapUnique(data));
  }

  return data;
}

// static
void DownloadItemWarningData::AddWarningActionEvent(DownloadItem* download,
                                                    WarningSurface surface,
                                                    WarningAction action) {
  if (!download) {
    RecordAddWarningActionEventOutcome(
        AddWarningActionEventOutcome::NOT_ADDED_MISSING_DOWNLOAD);
    return;
  }
  if (!download->IsDangerous()) {
    RecordAddWarningActionEventOutcome(
        AddWarningActionEventOutcome::NOT_ADDED_DOWNLOAD_NOT_DANGEROUS);
    return;
  }
  DownloadItemWarningData* data = GetOrCreate(download);
  if (action == WarningAction::SHOWN) {
    if (!data->logged_downloads_page_shown_ &&
        surface == WarningSurface::DOWNLOADS_PAGE) {
      base::UmaHistogramEnumeration(
          "Download.ShowedDownloadWarning.DownloadsPage",
          download->GetDangerType(), download::DOWNLOAD_DANGER_TYPE_MAX);
      data->logged_downloads_page_shown_ = true;
    }
    if (data->warning_first_shown_time_.is_null()) {
      RecordAddWarningActionEventOutcome(
          AddWarningActionEventOutcome::ADDED_WARNING_FIRST_SHOWN);
      RecordWarningActionAdded(action);
      data->warning_first_shown_time_ = base::Time::Now();
    } else {
      RecordAddWarningActionEventOutcome(
          AddWarningActionEventOutcome::NOT_ADDED_WARNING_SHOWN_ALREADY_LOGGED);
    }
    return;
  }
}

DownloadItemWarningData::DownloadItemWarningData() = default;

DownloadItemWarningData::~DownloadItemWarningData() = default;
