// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/browsing_history_service.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Time;

namespace history {

using HistoryEntry = BrowsingHistoryService::HistoryEntry;

void PrintTo(const HistoryEntry& entry, std::ostream* os) {
  *os << "{url: " << entry.url << ", time: " << entry.time
      << ", entry_type: " << entry.entry_type
      << ", is_actor_visit: " << entry.is_actor_visit << "}";
}

void PrintTo(const BrowsingHistoryService::QueryResultsInfo& info,
             std::ostream* os) {
  *os << "{reached_beginning: " << info.reached_beginning << "}";
}

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;

const char kUrl1[] = "http://www.one.com";
const char kUrl2[] = "http://www.two.com";
const char kUrl3[] = "http://www.three.com";
const char kUrl4[] = "http://www.four.com";

const HistoryEntry::EntryType kLocal = HistoryEntry::LOCAL_ENTRY;

Time OffsetToTimeWithBaseline(base::Time baseline_time, int64_t hour_offset) {
  return baseline_time + base::Hours(hour_offset);
}

struct TestResult {
  std::string url;
  int64_t hour_offset;  // Visit time in hours past the baseline time.
  HistoryEntry::EntryType type;
  VisitSource visit_source = VisitSource::SOURCE_BROWSED;
  bool is_actor_visit = false;
};

void PrintTo(const TestResult& result, std::ostream* os) {
  *os << "{url: " << result.url << ", hour_offset: " << result.hour_offset
      << ", type: " << result.type
      << ", visit_source: " << static_cast<int>(result.visit_source) << "}";
}

MATCHER_P2(MatchesHistory, baseline_time, expected, "") {
  return arg.url == GURL(expected.url) &&
         arg.time ==
             OffsetToTimeWithBaseline(baseline_time, expected.hour_offset) &&
         arg.entry_type == expected.type &&
         arg.is_actor_visit ==
             (expected.visit_source == VisitSource::SOURCE_ACTOR);
}

MATCHER_P3(MatchesQueryResult,
           baseline_time,
           reached_beginning,
           expected_entries,
           "") {
  const BrowsingHistoryService::QueryResultsInfo& info = arg.second;
  if (info.reached_beginning != reached_beginning) {
    *result_listener << "where reached_beginning should be "
                     << reached_beginning;
    return false;
  }

  const std::vector<BrowsingHistoryService::HistoryEntry>& entries = arg.first;
  std::vector<testing::Matcher<const HistoryEntry&>> matchers;
  for (const auto& entry : expected_entries) {
    matchers.push_back(MatchesHistory(baseline_time, entry));
  }
  return ExplainMatchResult(ElementsAreArray(matchers), entries,
                            result_listener);
}

class TestBrowsingHistoryDriver : public BrowsingHistoryDriver {
 public:
  using QueryResult = std::pair<std::vector<HistoryEntry>,
                                BrowsingHistoryService::QueryResultsInfo>;
  std::vector<QueryResult> GetQueryResults() { return query_results_; }

  int GetHistoryDeletedCount() { return history_deleted_count_; }

  void RunContinuation() {
    EXPECT_TRUE(continuation_closure_);
    std::move(continuation_closure_).Run();
  }

 private:
  // BrowsingHistoryDriver implementation.
  void OnQueryComplete(
      const std::vector<HistoryEntry>& results,
      const BrowsingHistoryService::QueryResultsInfo& query_results_info,
      base::OnceClosure continuation_closure) override {
    query_results_.push_back(QueryResult(results, query_results_info));
    continuation_closure_ = std::move(continuation_closure);
  }
  void OnRemoveVisitsComplete() override {}
  void OnRemoveVisitsFailed() override {}
  void OnRemoveVisits(
      const std::vector<ExpireHistoryArgs>& expire_list) override {}
  void HistoryDeleted() override { history_deleted_count_++; }
  bool AllowHistoryDeletions() override { return true; }

  int history_deleted_count_ = 0;
  std::vector<QueryResult> query_results_;
  base::OnceClosure continuation_closure_;
};

class BrowsingHistoryServiceTest : public ::testing::Test {
 protected:
  // Time::LocalMidnight() rounds _down_, which results in a time before
  // Time::UnixEpoch(). By adding 1 day we ensure all test data is after
  // Time::UnixEpoch().
  BrowsingHistoryServiceTest()
      : baseline_time_(Time::UnixEpoch().LocalMidnight() + base::Days(1)) {
    EXPECT_TRUE(history_dir_.CreateUniqueTempDir());
    local_history_ = CreateHistoryService(history_dir_.GetPath(), true);
    ResetService(driver(), local_history());
  }

  void ResetService(BrowsingHistoryDriver* driver,
                    HistoryService* local_history) {
    browsing_history_service_ =
        std::make_unique<BrowsingHistoryService>(driver, local_history);
  }

  void BlockUntilHistoryProcessesPendingRequests() {
    history::BlockUntilHistoryProcessesPendingRequests(local_history());
  }

  Time OffsetToTime(int64_t hour_offset) {
    return OffsetToTimeWithBaseline(baseline_time_, hour_offset);
  }

  void AddHistory(const std::vector<TestResult>& data) {
    for (const TestResult& entry : data) {
      VisitSource source =
          entry.is_actor_visit ? SOURCE_ACTOR : entry.visit_source;
      local_history()->AddPage(GURL(entry.url), OffsetToTime(entry.hour_offset),
                               source);
    }
  }

  HistoryEntry CreateEntry(const std::string& url,
                           const std::vector<int>& hour_offsets) {
    HistoryEntry entry;
    entry.url = GURL(url);
    for (int hour_offset : hour_offsets) {
      entry.all_timestamps[entry.url].insert(OffsetToTime(hour_offset));
    }
    return entry;
  }

  TestBrowsingHistoryDriver::QueryResult QueryHistory(size_t max_count = 0) {
    QueryOptions options;
    options.max_count = max_count;
    return QueryHistory(options);
  }

  TestBrowsingHistoryDriver::QueryResult QueryHistory(
      const QueryOptions& options) {
    return QueryHistory(std::u16string(), options);
  }

  TestBrowsingHistoryDriver::QueryResult QueryHistory(
      const std::u16string& query_text,
      const QueryOptions& options) {
    size_t previous_results_count = driver()->GetQueryResults().size();
    service()->QueryHistory(query_text, options);
    BlockUntilHistoryProcessesPendingRequests();
    const std::vector<TestBrowsingHistoryDriver::QueryResult> all_results =
        driver()->GetQueryResults();
    EXPECT_EQ(previous_results_count + 1, all_results.size());
    return *all_results.rbegin();
  }

  TestBrowsingHistoryDriver::QueryResult ContinueQuery() {
    size_t previous_results_count = driver()->GetQueryResults().size();
    driver()->RunContinuation();
    BlockUntilHistoryProcessesPendingRequests();
    const std::vector<TestBrowsingHistoryDriver::QueryResult> all_results =
        driver()->GetQueryResults();
    EXPECT_EQ(previous_results_count + 1, all_results.size());
    return *all_results.rbegin();
  }

  HistoryService* local_history() { return local_history_.get(); }
  TestBrowsingHistoryDriver* driver() { return &driver_; }
  BrowsingHistoryService* service() { return browsing_history_service_.get(); }

 protected:
  // Duplicates on the same day in the local timezone are removed, so set a
  // baseline time in local time.
  Time baseline_time_;

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir history_dir_;
  std::unique_ptr<HistoryService> local_history_;
  TestBrowsingHistoryDriver driver_;
  std::unique_ptr<BrowsingHistoryService> browsing_history_service_;
};


TEST_F(BrowsingHistoryServiceTest, QueryHistoryExcludes404s) {
  // Allow saving 404 visits to History.
  base::test::ScopedFeatureList scoped_featurelist;
  scoped_featurelist.InitAndEnableFeature(history::kVisitedLinksOn404);

  // Add a non-404 visit.
  AddHistory({{kUrl1, 1, kLocal}});

  // Add a 404 visit.
  HistoryAddPageArgs page_404_args;
  page_404_args.url = GURL(kUrl2);
  page_404_args.time = OffsetToTime(2);
  page_404_args.context_annotations = {.response_code = 404};
  local_history()->AddPage(page_404_args);

  BlockUntilHistoryProcessesPendingRequests();

  // 404s should be excluded from query results.
  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{{kUrl1, 1, kLocal}}));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryNoSources) {
  ResetService(driver(), nullptr);
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryJustLocal) {
  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryJustLocal) {
  AddHistory({{kUrl1, 1, kLocal}});
  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{{kUrl1, 1, kLocal}}));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryWithAppIdFilter) {
  AddHistory({{kUrl1, 1, kLocal}});

  QueryOptions options;
  options.app_id = "org.chromium.test";

  EXPECT_THAT(QueryHistory(options),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalTimeRanges) {
  AddHistory({{kUrl1, 1, kLocal},
              {kUrl2, 2, kLocal},
              {kUrl3, 3, kLocal},
              {kUrl4, 4, kLocal}});
  QueryOptions options;
  options.begin_time = OffsetToTime(2);
  options.end_time = OffsetToTime(4);
  // Having a `reached_beginning` value of false here seems
  // counterintuitive. Seems to be for paging by `begin_time` instead of
  // `count`. If the local history implementation changes, feel free to update
  // this value, all this test cares about is that BrowsingHistoryService passes
  // the values through correctly.
  EXPECT_THAT(QueryHistory(options),
              MatchesQueryResult(baseline_time_,
                                 /*reached_beginning*/ false,
                                 std::vector<TestResult>{
                                     {kUrl3, 3, kLocal},
                                     {kUrl2, 2, kLocal},
                                 }));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalPagingPartial) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  EXPECT_THAT(QueryHistory(2), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ false,
                                                  std::vector<TestResult>{
                                                      {kUrl3, 3, kLocal},
                                                      {kUrl2, 2, kLocal},
                                                  }));

  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{
                                                      {kUrl1, 1, kLocal},
                                                  }));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalPagingFull) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});
  // The first query returns all the local results and reaches the beginning.
  EXPECT_THAT(QueryHistory(3),
              MatchesQueryResult(baseline_time_,
                                 /*reached_beginning*/ true,
                                 std::vector<TestResult>{
                                     {kUrl3, 3, kLocal},
                                     {kUrl2, 2, kLocal},
                                     {kUrl1, 1, kLocal},
                                 }));

  EXPECT_THAT(ContinueQuery(), MatchesQueryResult(baseline_time_,
                                                  /*reached_beginning*/ true,
                                                  std::vector<TestResult>{}));
}

TEST_F(BrowsingHistoryServiceTest, RemoveVisitsFromLocalHistory) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}});
  BlockUntilHistoryProcessesPendingRequests();

  service()->RemoveVisits({CreateEntry(kUrl1, {1})});
  BlockUntilHistoryProcessesPendingRequests();

  EXPECT_THAT(QueryHistory(),
              MatchesQueryResult(baseline_time_, /*reached_beginning*/ true,
                                 std::vector<TestResult>{{kUrl2, 2, kLocal}}));
}

TEST_F(BrowsingHistoryServiceTest, IncludeActorVisits) {
  AddHistory({
      {kUrl1, 1, kLocal},
      {kUrl2, 2, kLocal, VisitSource::SOURCE_ACTOR},
  });

  QueryOptions options;
  options.include_actor_visits = true;
  EXPECT_THAT(
      QueryHistory(options),
      MatchesQueryResult(baseline_time_,
                         /*reached_beginning*/ true,
                         std::vector<TestResult>{
                             {kUrl2, 2, kLocal, VisitSource::SOURCE_ACTOR},
                             {kUrl1, 1, kLocal},
                         }));
}

TEST_F(BrowsingHistoryServiceTest, ActorVisitsExcludedByDefault) {
  AddHistory({
      {kUrl1, 1, kLocal},
      {kUrl2, 2, kLocal, VisitSource::SOURCE_ACTOR},
  });

  EXPECT_THAT(QueryHistory(), MatchesQueryResult(baseline_time_,
                                                 /*reached_beginning*/ true,
                                                 std::vector<TestResult>{
                                                     {kUrl1, 1, kLocal},
                                                 }));
}

TEST_F(BrowsingHistoryServiceTest, GroupSimilarVisits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowsingHistorySimilarVisitsGrouping);

  // Add a page with a custom title.
  HistoryAddPageArgs page1;
  page1.url = GURL("http://www.a.com/1");
  page1.time = OffsetToTime(4);
  page1.title = u"Title A";
  local_history()->AddPage(page1);

  // Add a page with a different URL but the same title as page 1, this should
  // not be grouped with page 1.
  HistoryAddPageArgs page2;
  page2.url = GURL("http://www.b.com/1");
  page2.time = OffsetToTime(3);
  page2.title = u"Title B";
  local_history()->AddPage(page2);

  // Add a page with a different URL but the same domain and title as page 1.
  // This should be grouped with page 1.
  HistoryAddPageArgs page3;
  page3.url = GURL("http://www.a.com/2");
  page3.time = OffsetToTime(2);
  page3.title = u"Title A";
  local_history()->AddPage(page3);

  // Add a page with a different URL and title, but the same domain as page 1,
  // this should not be grouped with page 1.
  HistoryAddPageArgs page4;
  page4.url = GURL("http://www.a.com/3");
  page4.time = OffsetToTime(1);
  page4.title = u"Title C";
  local_history()->AddPage(page4);

  // Add a page with the same URL and title as page 1, this should be grouped
  // with page 1.
  HistoryAddPageArgs page5;
  page5.url = GURL("http://www.a.com/1");
  page5.time = OffsetToTime(5);
  page5.title = u"Title A";
  local_history()->AddPage(page5);

  BlockUntilHistoryProcessesPendingRequests();

  EXPECT_THAT(
      QueryHistory(),
      MatchesQueryResult(baseline_time_, /*reached_beginning=*/true,
                         std::vector<TestResult>{
                             {"http://www.a.com/1", 5, kLocal},
                             {"http://www.b.com/1", 3, kLocal},
                             {"http://www.a.com/3", 1, kLocal},
                         }));
}

TEST_F(BrowsingHistoryServiceTest, ShouldQueryActorVisitsOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      history::kBrowsingHistoryActorIntegrationM3);

  AddHistory({{kUrl1, 1, kLocal},
              {kUrl2, 2, kLocal, VisitSource::SOURCE_BROWSED,
               true /*is_actor_visit*/}});

  QueryOptions options;
  options.include_user_visits = false;
  options.include_actor_visits = true;

  TestBrowsingHistoryDriver::QueryResult result = QueryHistory(options);

  ASSERT_EQ(1u, result.first.size());

  EXPECT_EQ(GURL(kUrl2), result.first[0].url);
  EXPECT_TRUE(result.first[0].is_actor_visit);
}

}  // namespace

}  // namespace history
