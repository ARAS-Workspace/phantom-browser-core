// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/browsing_history_service.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"

namespace history {

namespace {

QueryOptions OptionsWithEndTime(QueryOptions original_options,
                                base::Time end_time) {
  QueryOptions options(original_options);
  options.end_time = end_time;
  return options;
}

// The status of the result from a particular history source.
enum QuerySourceStatus {
  // Not a continuation and no response yet.
  UNINITIALIZED,
  // Could not query the particular source.
  NO_DEPENDENCY,
  // Successfully retrieved results, but there are more left.
  MORE_RESULTS,
  // Successfully retrieved results and we reached the end of results.
  REACHED_BEGINNING,
};

bool CanRetry(QuerySourceStatus status) {
  return status == UNINITIALIZED || status == MORE_RESULTS;
}

base::Time OldestTime(
    const std::vector<BrowsingHistoryService::HistoryEntry>& entries) {
  // If the vector is empty, then there is no oldest, so we use the oldest
  // possible time instead.
  if (entries.empty()) {
    return base::Time();
  }

  base::Time best = base::Time::Max();
  for (const BrowsingHistoryService::HistoryEntry& entry : entries) {
    best = std::min(best, entry.time);
  }
  return best;
}

}  // namespace

struct BrowsingHistoryService::QueryHistoryState
    : public base::RefCounted<BrowsingHistoryService::QueryHistoryState> {
  QueryHistoryState() = default;

  std::u16string search_text;
  QueryOptions original_options;

  QuerySourceStatus local_status = UNINITIALIZED;
  // Should always be sorted in reverse chronological order.
  std::vector<HistoryEntry> local_results;
  base::Time local_end_time_for_continuation;

 private:
  friend class base::RefCounted<BrowsingHistoryService::QueryHistoryState>;
  ~QueryHistoryState() = default;
};

BrowsingHistoryService::HistoryEntry::HistoryEntry(
    BrowsingHistoryService::HistoryEntry::EntryType entry_type,
    const GURL& url,
    const std::u16string& title,
    base::Time time,
    bool is_search_result,
    const std::u16string& snippet,
    bool blocked_visit,
    int visit_count,
    int typed_count,
    bool is_actor_visit,
    std::optional<std::string> app_id,
    history::VisitID visit_id)
    : entry_type(entry_type),
      url(url),
      title(title),
      time(time),
      is_search_result(is_search_result),
      snippet(snippet),
      blocked_visit(blocked_visit),
      visit_count(visit_count),
      typed_count(typed_count),
      is_actor_visit(is_actor_visit),
      app_id(app_id) {
  all_timestamps[url].insert(time);
  if (visit_id != history::kInvalidVisitID) {
    all_visit_ids.push_back(visit_id);
  }
}

BrowsingHistoryService::HistoryEntry::HistoryEntry()
    : entry_type(EMPTY_ENTRY), is_search_result(false), blocked_visit(false) {}

BrowsingHistoryService::HistoryEntry::HistoryEntry(const HistoryEntry& other) =
    default;

BrowsingHistoryService::HistoryEntry::~HistoryEntry() = default;

bool BrowsingHistoryService::HistoryEntry::SortByTimeDescending(
    const BrowsingHistoryService::HistoryEntry& entry1,
    const BrowsingHistoryService::HistoryEntry& entry2) {
  return entry1.time > entry2.time;
}

BrowsingHistoryService::QueryResultsInfo::~QueryResultsInfo() = default;

BrowsingHistoryService::BrowsingHistoryService(BrowsingHistoryDriver* driver,
                                               HistoryService* local_history)
    : driver_(driver),
      local_history_(local_history),
      clock_(new base::DefaultClock()) {
  DCHECK(driver_);

  // Get notifications when history is cleared.
  if (local_history_) {
    history_service_observation_.Observe(local_history_.get());
  }
}

BrowsingHistoryService::BrowsingHistoryService() = default;

BrowsingHistoryService::~BrowsingHistoryService() {
  query_task_tracker_.TryCancelAll();
}

void BrowsingHistoryService::QueryHistory(const std::u16string& search_text,
                                          const QueryOptions& options) {
  scoped_refptr<QueryHistoryState> state =
      base::MakeRefCounted<QueryHistoryState>();
  state->search_text = search_text;
  state->original_options = options;
  state->local_end_time_for_continuation = options.end_time;
  QueryHistoryInternal(std::move(state));
}

void BrowsingHistoryService::QueryHistoryInternal(
    scoped_refptr<QueryHistoryState> state) {
  // Anything in-flight is invalid.
  query_task_tracker_.TryCancelAll();

  bool should_return_results_immediately = true;
  const size_t desired_count =
      static_cast<size_t>(state->original_options.EffectiveMaxCount());

  if (local_history_) {
    if (state->local_results.size() < desired_count &&
        state->local_status != REACHED_BEGINNING) {
      CHECK_NE(state->local_status, NO_DEPENDENCY);
      should_return_results_immediately = false;
      local_history_->QueryHistory(
          state->search_text,
          OptionsWithEndTime(state->original_options,
                             state->local_end_time_for_continuation),
          base::BindOnce(&BrowsingHistoryService::QueryComplete,
                         weak_factory_.GetWeakPtr(), state),
          &query_task_tracker_);
    }
  } else {
    state->local_status = NO_DEPENDENCY;
  }

  if (should_return_results_immediately) {
    ReturnResultsToDriver(std::move(state));
  }
}

void BrowsingHistoryService::GetAllAppIds() {
  local_history_->GetAllAppIds(
      base::BindOnce(&BrowsingHistoryService::OnGetAllAppIds,
                     weak_factory_.GetWeakPtr()),
      &query_task_tracker_);
}

void BrowsingHistoryService::GetLastVisitToHostBeforeRecentNavigations(
    const std::string& host_name,
    base::OnceCallback<void(base::Time)> callback) {
  base::Time now = base::Time::Now();
  local_history_->GetLastVisitToHost(
      host_name, /*begin_time=*/base::Time(), /*end_time=*/now,
      VisitQuery404sPolicy::kExclude404s,
      base::BindOnce(
          &BrowsingHistoryService::OnLastVisitBeforeRecentNavigationsComplete,
          weak_factory_.GetWeakPtr(), host_name, now, std::move(callback)),
      &query_task_tracker_);
}

void BrowsingHistoryService::OnLastVisitBeforeRecentNavigationsComplete(
    const std::string& host_name,
    base::Time query_start_time,
    base::OnceCallback<void(base::Time)> callback,
    HistoryLastVisitResult result) {
  if (!result.success || result.last_visit.is_null()) {
    std::move(callback).Run(base::Time());
    return;
  }

  base::Time end_time =
      result.last_visit < (query_start_time - base::Minutes(1))
          ? result.last_visit
          : query_start_time - base::Minutes(1);
  local_history_->GetLastVisitToHost(
      host_name, /*begin_time=*/base::Time(), end_time,
      VisitQuery404sPolicy::kExclude404s,
      base::BindOnce(
          &BrowsingHistoryService::OnLastVisitBeforeRecentNavigationsComplete2,
          weak_factory_.GetWeakPtr(), std::move(callback)),
      &query_task_tracker_);
}

void BrowsingHistoryService::OnLastVisitBeforeRecentNavigationsComplete2(
    base::OnceCallback<void(base::Time)> callback,
    HistoryLastVisitResult result) {
  std::move(callback).Run(result.last_visit);
}

void BrowsingHistoryService::RemoveVisits(
    const std::vector<BrowsingHistoryService::HistoryEntry>& items) {
  if (delete_task_tracker_.HasTrackedTasks() ||
      !driver_->AllowHistoryDeletions()) {
    driver_->OnRemoveVisitsFailed();
    return;
  }

  std::vector<ExpireHistoryArgs> expire_list;
  expire_list.reserve(items.size());

  for (const BrowsingHistoryService::HistoryEntry& entry : items) {
    ExpireHistoryArgs* expire_args = nullptr;

    for (const auto& [url, timestamps] : entry.all_timestamps) {
      for (base::Time timestamp : timestamps) {
        if (!expire_args) {
          expire_list.resize(expire_list.size() + 1);
          expire_args = &expire_list.back();
          expire_args->SetTimeRangeForOneDay(timestamp);
          expire_args->urls.insert(url);
        }
      }
    }

    expire_args->restrict_app_id = entry.app_id;
  }

  if (local_history_) {
    local_history_->ExpireHistory(
        expire_list,
        base::BindOnce(&BrowsingHistoryService::RemoveComplete,
                       weak_factory_.GetWeakPtr()),
        &delete_task_tracker_);
  }

  driver_->OnRemoveVisits(expire_list);
}

std::vector<BrowsingHistoryService::HistoryEntry>
BrowsingHistoryService::GroupSimilarVisits(QueryHistoryState* state) {
  CHECK(state);

  // Will be used later to set the end time for the next query. This iterates
  // through each entry and makes no assumptions about their ordering.
  base::Time oldest_local = OldestTime(state->local_results);

  std::vector<HistoryEntry> sorted;
  sorted.assign(std::make_move_iterator(state->local_results.begin()),
                std::make_move_iterator(state->local_results.end()));
  state->local_results.clear();
  std::sort(sorted.begin(), sorted.end(), HistoryEntry::SortByTimeDescending);

  // Pre-reserve the size of the new vector. Since we're working with pointers
  // later on, not doing this could lead to the vector being resized and to
  // pointers to invalid locations.
  std::vector<HistoryEntry> grouped;
  grouped.reserve(sorted.size());

  // Maps the GroupingKey to the most recent entry on a particular day for
  // non-actor-initiated visits.
  std::map<GroupingKey, HistoryEntry*> non_actor_current_day_entries;
  // Same as above, but for actor-initiated visits.
  std::map<GroupingKey, HistoryEntry*> actor_current_day_entries;

  // Keeps track of the day that `*_current_day_entries` is holding
  // entries for in order to handle per-day grouping.
  base::Time current_day_midnight;

  for (HistoryEntry& entry : sorted) {
    // Reset the list of found entries when a visit from a new day is
    // encountered.
    if (current_day_midnight != entry.time.LocalMidnight()) {
      non_actor_current_day_entries.clear();
      actor_current_day_entries.clear();
      current_day_midnight = entry.time.LocalMidnight();
    }

    auto& current_day_entries = entry.is_actor_visit
                                    ? actor_current_day_entries
                                    : non_actor_current_day_entries;

    // TODO(b/481272035): Use the domain name that matches the displayed domain
    // name in the UI.
    GroupingKey key{.host = entry.url.GetHost(),
                    .title = entry.title,
                    .app_id = entry.app_id};

    // Keep this visit if it's the first visit of it's kind on the current day.
    auto [it, inserted] = current_day_entries.try_emplace(key, nullptr);
    if (inserted) {
      grouped.push_back(std::move(entry));
      it->second = &grouped.back();
    } else {
      HistoryEntry* matching_entry = it->second;

      // Merge all timestamps from the current entry into the matching entry.
      // This ensures all visits for similar URLs on the same day are tracked.
      for (const auto& [url, timestamps] : entry.all_timestamps) {
        matching_entry->all_timestamps[url].insert(timestamps.begin(),
                                                   timestamps.end());
      }
      matching_entry->all_visit_ids.insert(matching_entry->all_visit_ids.end(),
                                           entry.all_visit_ids.begin(),
                                           entry.all_visit_ids.end());

      // Aggregate visit and typed counts.
      matching_entry->visit_count += entry.visit_count;
      matching_entry->typed_count += entry.typed_count;

      // TODO(b/482947398): Aggregate all relevant HistoryEntry fields for
      // combined entries.
    }
  }

  if (state->local_status == MORE_RESULTS) {
    state->local_end_time_for_continuation = oldest_local;
  }
  return grouped;
}

void BrowsingHistoryService::QueryComplete(
    scoped_refptr<QueryHistoryState> state,
    QueryResults results) {
  std::vector<HistoryEntry>& output = state->local_results;
  output.reserve(output.size() + results.size());

  for (const auto& page : results) {
    output.emplace_back(
        HistoryEntry::LOCAL_ENTRY, page.url(), page.title(), page.visit_time(),
        !state->search_text.empty(), page.snippet().text(),
        page.blocked_visit(), page.visit_count(), page.typed_count(),
        page.has_actor_source(), page.app_id(), page.visit_id());
  }

  state->local_status =
      results.reached_beginning() ? REACHED_BEGINNING : MORE_RESULTS;

  ReturnResultsToDriver(std::move(state));
}

void BrowsingHistoryService::OnGetAllAppIds(GetAllAppIdsResult result) {
  driver_->OnGetAllAppIds(result.app_ids);
}

void BrowsingHistoryService::ReturnResultsToDriver(
    scoped_refptr<QueryHistoryState> state) {
  std::vector<HistoryEntry> results;
  bool group_visits = false;
  group_visits =
      base::FeatureList::IsEnabled(kBrowsingHistorySimilarVisitsGrouping);

  if (group_visits) {
    results = GroupSimilarVisits(state.get());
  } else {
    if (state->local_status == MORE_RESULTS && !state->local_results.empty()) {
      state->local_end_time_for_continuation =
          state->local_results.rbegin()->time;
    }
    results = std::move(state->local_results);
    state->local_results.clear();
  }

  RecordResultsMetrics(results);

  QueryResultsInfo info;
  info.search_text = state->search_text;
  info.reached_beginning = !CanRetry(state->local_status);
  base::OnceClosure continuation =
      base::BindOnce(&BrowsingHistoryService::QueryHistoryInternal,
                     weak_factory_.GetWeakPtr(), std::move(state));
  driver_->OnQueryComplete(results, info, std::move(continuation));
}

void BrowsingHistoryService::RecordResultsMetrics(
    const std::vector<HistoryEntry>& results) {
  // Count the number of entries, split by entries before vs after the local
  // expiry threshold (90 days).
  const base::Time local_expiry_threshold =
      clock_->Now() - base::Days(HistoryBackend::kExpireDaysThreshold);
  size_t pre_expiry_count = 0;
  size_t post_expiry_count = 0;
  for (const HistoryEntry& entry : results) {
    if (entry.time < local_expiry_threshold) {
      ++pre_expiry_count;
    } else {
      ++post_expiry_count;
    }
  }

  // Note: The histogram max of 150 is chosen to match `RESULTS_PER_PAGE` from
  // chrome/browser/resources/history/constants.ts and `kMaxQueryCount` from
  // chrome/browser/android/history/browsing_history_bridge.cc.
  base::UmaHistogramCustomCounts(
      "History.BrowsingHistoryResult.LocalOnly.PreExpiryThreshold",
      pre_expiry_count, 0, 150, 50);
  base::UmaHistogramCustomCounts(
      "History.BrowsingHistoryResult.LocalOnly.PostExpiryThreshold",
      post_expiry_count, 0, 150, 50);

  RecordDuplicateVisitsCount(results);
}

void BrowsingHistoryService::RecordDuplicateVisitsCount(
    const std::vector<HistoryEntry>& results) {
  int duplicate_visits_count = 0;
  for (const HistoryEntry& entry : results) {
    for (const auto& [url, timestamps] : entry.all_timestamps) {
      // Omit the timestamp for the entry itself from the duplicate count.
      url == entry.url ? duplicate_visits_count += timestamps.size() - 1
                        : duplicate_visits_count += timestamps.size();
    }
  }

  // Note: The histogram max of 150 is chosen to match `RESULTS_PER_PAGE` from
  // chrome/browser/resources/history/constants.ts and `kMaxQueryCount` from
  // chrome/browser/android/history/browsing_history_bridge.cc.
  base::UmaHistogramCustomCounts(
      "History.BrowsingHistoryResult.DuplicateVisitsCount",
      duplicate_visits_count, 0, 150, 50);
}

void BrowsingHistoryService::RemoveComplete() {
  // Notify the driver that the deletion request is complete.
  driver_->OnRemoveVisitsComplete();
}

void BrowsingHistoryService::OnHistoryDeletions(
    HistoryService* history_service,
    const DeletionInfo& deletion_info) {
  // TODO(calamity): Only ignore history deletions when they are actually
  // initiated by us, rather than ignoring them whenever we are deleting.
  if (!delete_task_tracker_.HasTrackedTasks()) {
    driver_->HistoryDeleted();
  }
}

}  // namespace history
