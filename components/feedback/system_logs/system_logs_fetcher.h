// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_
#define COMPONENTS_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Callback that the SystemLogsFetcher uses to return data.
using SysLogsFetcherCallback =
    base::OnceCallback<void(std::unique_ptr<SystemLogsResponse>)>;

// The SystemLogsFetcher fetches key-value data from a list of log sources.
//
// EXAMPLE:
// class Example {
//  public:
//   void ProcessLogs(std::unique_ptr<SystemLogsResponse> response) {
//      // do something with the logs
//   }
//   void GetLogs() {
//     SystemLogsFetcher* fetcher = new SystemLogsFetcher();
//     fetcher->AddSource(std::make_unique<LogSourceOne>());
//     fetcher->AddSource(std::make_unique<LogSourceTwo>());
//     fetcher->Fetch(base::BindOnce(&Example::ProcessLogs, this));
//   }
// };
class SystemLogsFetcher {
 public:
  SystemLogsFetcher();

  SystemLogsFetcher(const SystemLogsFetcher&) = delete;
  SystemLogsFetcher& operator=(const SystemLogsFetcher&) = delete;

  ~SystemLogsFetcher();

  // Adds a source to use when fetching.
  void AddSource(std::unique_ptr<SystemLogsSource> source);

  // Starts the fetch process. After the fetch completes, this instance calls
  // |callback|, then schedules itself to be deleted.
  void Fetch(SysLogsFetcherCallback callback);

 private:
  // Callback passed to all the data sources. Calls AddResponse().
  void OnFetched(const std::string& source_name,
                 std::unique_ptr<SystemLogsResponse> response);

  // Merges the |response| it receives into response_. When all the data sources
  // have responded, it deletes their objects and returns the response to the
  // callback_. After this it deletes this instance of the object.
  void AddResponse(const std::string& source_name,
                   std::unique_ptr<SystemLogsResponse> response);

  // Runs the callback provided to Fetch and posts a task to delete |this|.
  void RunCallbackAndDeleteSoon();

  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<std::unique_ptr<SystemLogsSource>> data_sources_;
  SysLogsFetcherCallback callback_;

  std::unique_ptr<SystemLogsResponse> response_;  // The actual response data.
  size_t num_pending_requests_;  // The number of callbacks it should get.

  base::WeakPtrFactory<SystemLogsFetcher> weak_ptr_factory_{this};
};

}  // namespace system_logs

#endif  // COMPONENTS_FEEDBACK_SYSTEM_LOGS_SYSTEM_LOGS_FETCHER_H_
