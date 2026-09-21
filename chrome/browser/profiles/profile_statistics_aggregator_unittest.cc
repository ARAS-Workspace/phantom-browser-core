// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics_aggregator.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/test/test_file_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BookmarkStatHelper {
 public:
  void StatsCallback(profiles::ProfileCategoryStats stats) {
    if (stats.back().category == profiles::kProfileStatisticsBookmarks) {
      ++num_of_times_called_;
    }
  }

  int GetNumOfTimesCalled() { return num_of_times_called_; }

 private:
  int num_of_times_called_ = 0;
};
}  // namespace

class ProfileStatisticsAggregatorTest : public testing::Test {
 public:
  ProfileStatisticsAggregatorTest() {
    history_service_.Init(history::HistoryDatabaseParams(
        base::CreateUniqueTempDirectoryScopedToTest(),
        /*download_interrupt_reason_none=*/0,
        /*download_interrupt_reason_crash=*/0, version_info::Channel::UNKNOWN));
  }

  std::unique_ptr<ProfileStatisticsAggregator> CreateAggregator(
      base::OnceClosure done_callback) {
    return std::make_unique<ProfileStatisticsAggregator>(
        &bookmark_model_, &history_service_, std::move(done_callback));
  }

  bookmarks::BookmarkModel* bookmark_model() { return &bookmark_model_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  bookmarks::BookmarkModel bookmark_model_{
      std::make_unique<bookmarks::TestBookmarkClient>()};
  history::HistoryService history_service_;
};

TEST_F(ProfileStatisticsAggregatorTest, WaitOrCountBookmarks) {
  // Run ProfileStatisticsAggregator::WaitOrCountBookmarks.
  BookmarkStatHelper bookmark_stat_helper;
  base::RunLoop run_loop_aggregator_done;

  std::unique_ptr<ProfileStatisticsAggregator> aggregator =
      CreateAggregator(run_loop_aggregator_done.QuitClosure());
  aggregator->AddCallbackAndStartAggregator(
      base::BindRepeating(&BookmarkStatHelper::StatsCallback,
                          base::Unretained(&bookmark_stat_helper)));

  // Wait until ProfileStatisticsAggregator::WaitOrCountBookmarks is run.
  base::RunLoop run_loop1;
  run_loop1.RunUntilIdle();
  EXPECT_EQ(0, bookmark_stat_helper.GetNumOfTimesCalled());

  // Run ProfileStatisticsAggregator::WaitOrCountBookmarks again.
  aggregator->AddCallbackAndStartAggregator(
      profiles::ProfileStatisticsCallback());
  // Wait until ProfileStatisticsAggregator::WaitOrCountBookmarks is run.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_EQ(0, bookmark_stat_helper.GetNumOfTimesCalled());

  // Load the bookmark model. When the model is loaded (asynchronously), the
  // observer added by WaitOrCountBookmarks is run.
  bookmark_model()->LoadEmptyForTest();

  run_loop_aggregator_done.Run();
  EXPECT_EQ(1, bookmark_stat_helper.GetNumOfTimesCalled());
}
