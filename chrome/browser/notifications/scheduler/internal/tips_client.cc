// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/tips_client.h"

#include <utility>

#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/notifications/scheduler/internal/stats.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/notifications/scheduler/public/tips_agent.h"
#include "chrome/browser/tips/core/tips_types.h"
#include "chrome/browser/tips/core/tips_utils.h"

namespace notifications {

TipsClient::TipsClient(std::unique_ptr<TipsAgent> tips_agent,
                       PrefService* pref_service)
    : tips_agent_(std::move(tips_agent)), pref_service_(pref_service) {}

TipsClient::~TipsClient() = default;

void TipsClient::BeforeShowNotification(
    std::unique_ptr<NotificationData> notification_data,
    NotificationDataCallback callback) {
  std::move(callback).Run(std::move(notification_data));
}

void TipsClient::OnShowNotification(
    std::unique_ptr<NotificationData> notification_data) {
  NOTIMPLEMENTED();
}

void TipsClient::OnSchedulerInitialized(bool success,
                                        std::set<std::string> guids) {}

void TipsClient::OnUserAction(const UserActionData& action_data) {}

void TipsClient::GetThrottleConfig(
    ThrottleConfigCallback callback) {
  std::move(callback).Run(nullptr);
}

}  // namespace notifications
