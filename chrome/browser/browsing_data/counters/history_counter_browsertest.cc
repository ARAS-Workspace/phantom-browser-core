// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/history_counter.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace {

using browsing_data::BrowsingDataCounter;
using browsing_data::HistoryCounter;

class HistoryCounterTest : public InProcessBrowserTest {
 public:
  HistoryCounterTest() = default;
  ~HistoryCounterTest() override = default;

  void SetUpOnMainThread() override {
    time_ = base::Time::Now();
    history_service_ = HistoryServiceFactory::GetForProfileWithoutCreating(
        browser()->GetProfile());

    SetHistoryDeletionPref(true);
    SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  }

  void TearDownOnMainThread() override { history_service_ = nullptr; }

  void AddVisit(std::string_view url) {
    history_service_->AddPage(GURL(url), time_, history::SOURCE_BROWSED);
  }

  void SetTime(base::Time time) { time_ = time; }

  void RevertTimeInDays(int days) { time_ -= base::Days(days); }

  void SetHistoryDeletionPref(bool value) {
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        browsing_data::prefs::kDeleteBrowsingHistory, value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    browser()->GetProfile()->GetPrefs()->SetInteger(
        browsing_data::prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  void WaitForCounting() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  std::string GetLastVisitedDomain() {
    DCHECK(finished_);
    return last_visited_domain_;
  }

  BrowsingDataCounter::ResultInt GetUniqueDomainsResult() {
    DCHECK(finished_);
    return unique_domains_result_;
  }

  void Callback(std::unique_ptr<BrowsingDataCounter::Result> result) {
    finished_ = result->Finished();

    if (finished_) {
      auto* history_result =
          static_cast<HistoryCounter::HistoryResult*>(result.get());

      unique_domains_result_ = history_result->Value();
      last_visited_domain_ = history_result->last_visited_domain();
    }

    if (run_loop_ && finished_) {
      run_loop_->Quit();
    }
  }

  history::HistoryService* GetHistoryService() { return history_service_; }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  raw_ptr<history::HistoryService> history_service_ = nullptr;
  base::Time time_;

  bool finished_;
  std::string last_visited_domain_;
  BrowsingDataCounter::ResultInt unique_domains_result_;
};

// Tests that the counter considers duplicate visits from the same day
// to be a single item.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, DuplicateVisits) {
  // Start at a fixed day to avoid flakiness due to timezone changes.
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("1 Jul 2020 10:00 GMT", &time));
  SetTime(time);

  AddVisit("https://www.google.com");  // 1 visit and 1 unique domain
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");  // 2 visits and 2 unique domains
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.example.com");  // 3 visits

  RevertTimeInDays(1);
  AddVisit("https://www.google.com");   // 4 visits
  AddVisit("https://www.example.com");  // 5 visits and 3 unique domains
  AddVisit("https://www.example.com");

  RevertTimeInDays(1);
  AddVisit("https://www.chrome.com");  // 6 visits
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.google.com");  // 7 visits
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.google.com");
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");

  Profile* profile = browser()->GetProfile();

  HistoryCounter counter(GetHistoryService());

  counter.Init(profile->GetPrefs(),
               base::BindRepeating(&HistoryCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ("example.com", GetLastVisitedDomain());
  EXPECT_EQ(3u, GetUniqueDomainsResult());
}

// Tests that the counter reports the local visits.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, LocalVisits) {
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");

  Profile* profile = browser()->GetProfile();

  browsing_data::HistoryCounter counter(GetHistoryService());

  counter.Init(profile->GetPrefs(),
               base::BindRepeating(&HistoryCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ("chrome.com", GetLastVisitedDomain());
  EXPECT_EQ(2u, GetUniqueDomainsResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, PrefChanged) {
  SetHistoryDeletionPref(false);
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");

  Profile* profile = browser()->GetProfile();

  HistoryCounter counter(GetHistoryService());

  counter.Init(profile->GetPrefs(),
               base::BindRepeating(&HistoryCounterTest::Callback,
                                   base::Unretained(this)));
  SetHistoryDeletionPref(true);

  WaitForCounting();
  EXPECT_EQ("chrome.com", GetLastVisitedDomain());
  EXPECT_EQ(2u, GetUniqueDomainsResult());
}

// Tests that changing the deletion period restarts the counting, and that
// the result takes visit dates into account.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, PeriodChanged) {
  AddVisit("https://www.google.com");

  RevertTimeInDays(2);
  AddVisit("https://www.google.com");
  AddVisit("https://www.example.com");

  RevertTimeInDays(4);
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.example.com");

  RevertTimeInDays(20);
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.example.com");

  RevertTimeInDays(10);
  AddVisit("https://www.example.com");
  AddVisit("https://www.example.com");
  AddVisit("https://www.example.com");

  RevertTimeInDays(100);
  AddVisit("https://www.google.com");
  AddVisit("https://www.example.com");
  AddVisit("https://www.example.com");

  Profile* profile = browser()->GetProfile();

  HistoryCounter counter(GetHistoryService());

  counter.Init(profile->GetPrefs(),
               base::BindRepeating(&HistoryCounterTest::Callback,
                                   base::Unretained(this)));

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  WaitForCounting();
  EXPECT_EQ("google.com", GetLastVisitedDomain());
  EXPECT_EQ(1u, GetUniqueDomainsResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_DAY);
  WaitForCounting();
  EXPECT_EQ("google.com", GetLastVisitedDomain());
  EXPECT_EQ(1u, GetUniqueDomainsResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_WEEK);
  WaitForCounting();
  EXPECT_EQ("google.com", GetLastVisitedDomain());
  EXPECT_EQ(3u, GetUniqueDomainsResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::FOUR_WEEKS);
  WaitForCounting();
  EXPECT_EQ("google.com", GetLastVisitedDomain());
  EXPECT_EQ(3u, GetUniqueDomainsResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  WaitForCounting();
  EXPECT_EQ("google.com", GetLastVisitedDomain());
  EXPECT_EQ(3u, GetUniqueDomainsResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::OLDER_THAN_30_DAYS);
  WaitForCounting();
  EXPECT_EQ("example.com", GetLastVisitedDomain());
  EXPECT_EQ(2u, GetUniqueDomainsResult());
}

}  // namespace
