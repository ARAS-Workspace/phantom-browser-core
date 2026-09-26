// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prompt_status.h"
#include "components/download/public/common/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Return;
using ::testing::ReturnRefOfCopy;

namespace {

constexpr char kDownloadCancelReasonHistogram[] = "Download.CancelReason";

TEST(DownloadStatsTest, RecordDownloadCancelReason) {
  base::HistogramTester histogram_tester;
  RecordDownloadCancelReason(DownloadCancelReason::kTargetConfirmationResult);
  histogram_tester.ExpectBucketCount(
      kDownloadCancelReasonHistogram,
      DownloadCancelReason::kTargetConfirmationResult, 1);
  histogram_tester.ExpectTotalCount(kDownloadCancelReasonHistogram, 1);
}

TEST(DownloadStatsTest, RecordDownloadOpen) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  RecordDownloadOpen(DOWNLOAD_OPEN_METHOD_DEFAULT_BROWSER, "application/pdf");

  EXPECT_EQ(1, user_action_tester.GetActionCount("Download.Open"));
  histogram_tester.ExpectUniqueSample(
      "Download.OpenMethod",
      /*sample=*/DOWNLOAD_OPEN_METHOD_DEFAULT_BROWSER,
      /*expected_bucket_count=*/1);
}

}  // namespace
