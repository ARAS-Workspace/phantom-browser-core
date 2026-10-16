// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/system_logs/system_logs_fetcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"

namespace system_logs {

SystemLogsFetcher::SystemLogsFetcher()
    : response_(std::make_unique<SystemLogsResponse>()),
      num_pending_requests_(0) {}

SystemLogsFetcher::~SystemLogsFetcher() = default;

void SystemLogsFetcher::AddSource(std::unique_ptr<SystemLogsSource> source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_sources_.emplace_back(std::move(source));
  num_pending_requests_++;
}

void SystemLogsFetcher::Fetch(SysLogsFetcherCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  callback_ = std::move(callback);

  if (data_sources_.empty()) {
    RunCallbackAndDeleteSoon();
    return;
  }

  for (size_t i = 0; i < data_sources_.size(); ++i) {
    VLOG(1) << "Fetching SystemLogSource: " << data_sources_[i]->source_name();
    data_sources_[i]->Fetch(base::BindOnce(&SystemLogsFetcher::OnFetched,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           data_sources_[i]->source_name()));
  }
}

void SystemLogsFetcher::OnFetched(
    const std::string& source_name,
    std::unique_ptr<SystemLogsResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(response);

  VLOG(1) << "Received SystemLogSource: " << source_name;

  AddResponse(source_name, std::move(response));
}

void SystemLogsFetcher::AddResponse(
    const std::string& source_name,
    std::unique_ptr<SystemLogsResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& it : *response) {
    // An element with a duplicate key would not be successfully inserted.
    bool ok = response_->emplace(it).second;
    DCHECK(ok) << "Duplicate key found: " << it.first;
  }

  --num_pending_requests_;
  if (num_pending_requests_ > 0)
    return;

  RunCallbackAndDeleteSoon();
}

void SystemLogsFetcher::RunCallbackAndDeleteSoon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(std::move(response_));
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
}

}  // namespace system_logs
