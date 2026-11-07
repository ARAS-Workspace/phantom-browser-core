// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_BROWSING_HISTORY_SERVICE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_BROWSING_HISTORY_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/url_row.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"

namespace history {

class BrowsingHistoryDriver;
class QueryResults;
struct QueryOptions;

// Interacts with HistoryService to query history and provide results to the
// associated BrowsingHistoryDriver.
class BrowsingHistoryService : public HistoryServiceObserver {
 public:
  // Represents a history entry to be shown to the user. A single entry can
  // represent multiple visits, since only the most recent visit on a particular
  // day is shown.
  struct HistoryEntry {
    // Values indicating whether an entry represents local visits.
    enum EntryType {
      EMPTY_ENTRY = 0,
      LOCAL_ENTRY,
    };

    HistoryEntry(EntryType type,
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
                 history::VisitID visit_id);
    HistoryEntry();
    HistoryEntry(const HistoryEntry& other);
    virtual ~HistoryEntry();

    // Comparison function for sorting HistoryEntries from newest to oldest.
    static bool SortByTimeDescending(const HistoryEntry& entry1,
                                     const HistoryEntry& entry2);

    // The type of visits this entry represents.
    EntryType entry_type;

    GURL url;

    std::u16string title;  // Title of the entry. May be empty.

    // The time of the entry. Usually this will be the time of the most recent
    // visit to `url` on a particular day as defined in the local timezone.
    base::Time time;

    // Timestamps of all visits to this or similar URLs on the same day. Similar
    // URLs are ones with matching title and host.
    absl::flat_hash_map<GURL, std::set<base::Time>> all_timestamps;

    // If true, this entry is a search result.
    bool is_search_result;

    // The entry's search snippet, if this entry is a search result.
    std::u16string snippet;

    // Whether this entry was blocked when it was attempted.
    bool blocked_visit;

    // Total number of times this URL has been visited.
    int visit_count = 0;

    // Number of times this URL has been manually entered in the URL bar.
    int typed_count = 0;

    // Whether the visit is actor-initiated.
    bool is_actor_visit = false;

    // ID of the app this entry was generated for. Set to a non-null value
    // on Android only.
    std::optional<std::string> app_id;

    // All visit IDs in the local database represented by this entry (including
    // synced visits).
    std::vector<history::VisitID> all_visit_ids;
  };

  // Contains information about a completed history query.
  struct QueryResultsInfo {
    ~QueryResultsInfo();

    // The query search text.
    std::u16string search_text;

    // Whether this query reached the end of all results, or if there are more
    // history entries that can be fetched through paging.
    bool reached_beginning = false;
  };

  BrowsingHistoryService(BrowsingHistoryDriver* driver,
                         HistoryService* local_history);

  BrowsingHistoryService(const BrowsingHistoryService&) = delete;
  BrowsingHistoryService& operator=(const BrowsingHistoryService&) = delete;

  ~BrowsingHistoryService() override;

  // Start a new query with the given parameters.
  virtual void QueryHistory(const std::u16string& search_text,
                            const QueryOptions& options);

  // Fetch all the app IDs used in the database.
  void GetAllAppIds();

  // Callback invoked when the app ID fetching task is completed.
  void OnGetAllAppIds(GetAllAppIdsResult result);

  // Gets a version of the last time any webpage on the given host was visited,
  // by using the min("last navigation time", x minutes ago) as the upper bound
  // of the GetLastVisitToHost query. This is done in order to provide the user
  // with a more useful sneak peak into their navigation history, by excluding
  // the site(s) they were just on.
  void GetLastVisitToHostBeforeRecentNavigations(
      const std::string& host_name,
      base::OnceCallback<void(base::Time)> callback);

  // Removes `items` from history.
  // TODO(tommycli): Update this API to take only URLs and timestamps, because
  // callers only have that information, and only that information is used by
  // the actual implementation.
  void RemoveVisits(const std::vector<HistoryEntry>& items);

 protected:
  // Should be used only for tests when mocking the service.
  BrowsingHistoryService();

 private:
  // Used to hold and track query state between asynchronous calls.
  struct QueryHistoryState;

  // Used for grouping similar history entries.
  struct GroupingKey {
    std::string host;
    std::u16string title;
    std::optional<std::string> app_id;

    auto operator<=>(const GroupingKey& other) const = default;
  };

  // Returns the local results from `state` in reverse chronological order. Any
  // results with the same host and title will be merged together for each day.
  // This function also updates the end time in `state` that the next query
  // should be made against.
  static std::vector<HistoryEntry> GroupSimilarVisits(QueryHistoryState* state);

  // Core implementation of history querying.
  void QueryHistoryInternal(scoped_refptr<QueryHistoryState> state);

  // Callback from the history system when a history query has completed.
  void QueryComplete(scoped_refptr<QueryHistoryState> state,
                     QueryResults results);

  // Callback from the history system when the last visit query has completed.
  // May need to do a second query based on the results.
  void OnLastVisitBeforeRecentNavigationsComplete(
      const std::string& host_name,
      base::Time query_start_time,
      base::OnceCallback<void(base::Time)> callback,
      HistoryLastVisitResult result);

  // Callback from the history system when the last visit query has completed
  // the second time.
  void OnLastVisitBeforeRecentNavigationsComplete2(
      base::OnceCallback<void(base::Time)> callback,
      HistoryLastVisitResult result);

  // Sends the query results from the local history database to the
  // BrowsingHistoryDriver.
  void ReturnResultsToDriver(scoped_refptr<QueryHistoryState> state);

  void RecordResultsMetrics(const std::vector<HistoryEntry>& results);

  // Records the number of duplicate visits removed from a history query.
  static void RecordDuplicateVisitsCount(
      const std::vector<HistoryEntry>& results);

  // Callback from the history system when visits were deleted.
  void RemoveComplete();

  // HistoryServiceObserver implementation.
  void OnHistoryDeletions(HistoryService* history_service,
                          const DeletionInfo& deletion_info) override;

  // Tracker for search requests to the history service.
  base::CancelableTaskTracker query_task_tracker_;

  // Tracker for delete requests to the history service.
  base::CancelableTaskTracker delete_task_tracker_;

  // HistoryService (local history) observer.
  base::ScopedObservation<HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  raw_ptr<BrowsingHistoryDriver, DanglingUntriaged> driver_;

  raw_ptr<HistoryService, DanglingUntriaged> local_history_;

  // The clock used to vend times.
  std::unique_ptr<base::Clock> clock_;

  base::WeakPtrFactory<BrowsingHistoryService> weak_factory_{this};
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_BROWSING_HISTORY_SERVICE_H_
