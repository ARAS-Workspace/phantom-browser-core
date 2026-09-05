// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics.h"

#include <utility>

#include "chrome/browser/profiles/profile_statistics_aggregator.h"

ProfileStatistics::ProfileStatistics(bookmarks::BookmarkModel* bookmark_model,
                                     history::HistoryService* history_service)
    : bookmark_model_(bookmark_model),
      history_service_(history_service),
      aggregator_(nullptr) {}

ProfileStatistics::~ProfileStatistics() = default;

void ProfileStatistics::GatherStatistics(
    profiles::ProfileStatisticsCallback callback) {
  if (!aggregator_) {
    aggregator_ = std::make_unique<ProfileStatisticsAggregator>(
        bookmark_model_, history_service_,
        base::BindOnce(&ProfileStatistics::DeregisterAggregator,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  aggregator_->AddCallbackAndStartAggregator(std::move(callback));
}

void ProfileStatistics::DeregisterAggregator() {
  aggregator_ = nullptr;
}
