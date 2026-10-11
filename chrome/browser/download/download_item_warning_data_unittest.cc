// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_warning_data.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/download/public/common/mock_download_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using WarningSurface = DownloadItemWarningData::WarningSurface;
using WarningAction = DownloadItemWarningData::WarningAction;
using ::testing::Return;

class DownloadItemWarningDataTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  download::MockDownloadItem download_;
};

TEST_F(DownloadItemWarningDataTest, GetEvents_NotDangerous) {
  base::HistogramTester histograms;
  ON_CALL(download_, IsDangerous()).WillByDefault(Return(false));
  DownloadItemWarningData::AddWarningActionEvent(
      &download_, WarningSurface::BUBBLE_MAINPAGE, WarningAction::SHOWN);
  DownloadItemWarningData::AddWarningActionEvent(
      &download_, WarningSurface::BUBBLE_SUBPAGE, WarningAction::PROCEED);

  // The events are not added because the download is not dangerous.
  histograms.ExpectUniqueSample(
      "Download.WarningData.AddWarningActionEventOutcome2",
      /*NOT_ADDED_DOWNLOAD_NOT_DANGEROUS=*/6, 2);
}
