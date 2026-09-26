// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/reporting_service.h"

#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/test/task_environment.h"
#include "components/metrics/log_store.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_scheduler.h"
#include "components/metrics/metrics_upload_scheduler.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "crypto/obsolete/sha1.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {

namespace {

// Represent a flushed log and its metadata to be used for testing.
struct TestLog {
  explicit TestLog(const std::string& log) : log(log), user_id(std::nullopt) {}
  TestLog(const std::string& log, uint64_t user_id)
      : log(log), user_id(user_id) {}
  TestLog(const std::string& log, uint64_t user_id, LogMetadata log_metadata)
      : log(log), user_id(user_id), log_metadata(log_metadata) {}
  TestLog(const TestLog& other) = default;
  ~TestLog() = default;

  const std::string log;
  const std::optional<uint64_t> user_id;
  const LogMetadata log_metadata;
};

const char kTestUploadUrl[] = "test_url";
const char kTestMimeType[] = "test_mime_type";

class TestLogStore : public LogStore {
 public:
  TestLogStore() = default;
  ~TestLogStore() override = default;

  void AddLog(const TestLog& log) { logs_.push_back(log); }

  // LogStore:
  bool has_unsent_logs() const override { return !logs_.empty(); }
  bool has_staged_log() const override { return !staged_log_hash_.empty(); }
  const std::string& staged_log() const override { return logs_.front().log; }
  const std::string& staged_log_hash() const override {
    return staged_log_hash_;
  }
  std::optional<uint64_t> staged_log_user_id() const override {
    return logs_.front().user_id;
  }
  const LogMetadata staged_log_metadata() const override {
    return logs_.front().log_metadata;
  }
  const std::string& staged_log_signature() const override {
    return base::EmptyString();
  }
  void StageNextLog() override {
    if (has_unsent_logs()) {
      staged_log_hash_ = metrics::Sha1ForUnsentLogStore(logs_.front().log);
    }
  }
  void DiscardStagedLogImpl(std::string_view reason) override {
    if (!has_staged_log()) {
      return;
    }
    logs_.pop_front();
    staged_log_hash_.clear();
  }
  void MarkStagedLogAsSent() override {}
  void TrimAndPersistUnsentLogs(bool overwrite_in_memory_store) override {}
  void LoadPersistedUnsentLogs() override {}

 private:
  std::string staged_log_hash_;
  std::deque<TestLog> logs_;
};

class TestReportingService : public ReportingService {
 public:
  TestReportingService(MetricsServiceClient* client, PrefService* local_state)
      : ReportingService(client,
                         local_state,
                         100,
                         /*logs_event_manager=*/nullptr,
                         background_task::TaskIds::UMA_UPLOAD_JOB_ID) {
    Initialize();
  }

  TestReportingService(const TestReportingService&) = delete;
  TestReportingService& operator=(const TestReportingService&) = delete;

  ~TestReportingService() override = default;

  void AddLog(const TestLog& log) { log_store_.AddLog(log); }
  bool HasUnsentLogs() { return log_store_.has_unsent_logs(); }

 private:
  // ReportingService:
  LogStore* log_store() override { return &log_store_; }
  GURL GetUploadUrl() const override { return GURL(kTestUploadUrl); }
  GURL GetInsecureUploadUrl() const override { return GURL(kTestUploadUrl); }
  std::string_view upload_mime_type() const override { return kTestMimeType; }
  MetricsLogUploader::MetricServiceType service_type() const override {
    return MetricsLogUploader::MetricServiceType::UMA;
  }

  TestLogStore log_store_;
};

class ReportingServiceTest : public testing::Test {
 public:
  ReportingServiceTest() {
    ReportingService::RegisterPrefs(testing_local_state_.registry());
  }

  ReportingServiceTest(const ReportingServiceTest&) = delete;
  ReportingServiceTest& operator=(const ReportingServiceTest&) = delete;

  ~ReportingServiceTest() override = default;

  PrefService* GetLocalState() { return &testing_local_state_; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestMetricsServiceClient client_;

 private:
  TestingPrefServiceSimple testing_local_state_;
};

}  // namespace

TEST_F(ReportingServiceTest, BasicTest) {
  TestReportingService service(&client_, GetLocalState());
  service.AddLog(TestLog("log1"));
  service.AddLog(TestLog("log2"));

  service.EnableReporting();
  task_environment_.FastForwardBy(
      base::Seconds(MetricsScheduler::GetInitialIntervalSeconds()));
  EXPECT_TRUE(client_.uploader()->is_uploading());
  EXPECT_EQ(1, client_.uploader()->reporting_info().attempt_count());
  EXPECT_FALSE(client_.uploader()->reporting_info().has_last_response_code());

  client_.uploader()->CompleteUpload(404);
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetInitialBackoffInterval());
  EXPECT_TRUE(client_.uploader()->is_uploading());
  EXPECT_EQ(2, client_.uploader()->reporting_info().attempt_count());
  EXPECT_EQ(404, client_.uploader()->reporting_info().last_response_code());

  client_.uploader()->CompleteUpload(200);
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetUnsentLogsInterval());
  EXPECT_TRUE(client_.uploader()->is_uploading());
  EXPECT_EQ(1, client_.uploader()->reporting_info().attempt_count());
  EXPECT_EQ(200, client_.uploader()->reporting_info().last_response_code());

  client_.uploader()->CompleteUpload(200);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0U);
  EXPECT_FALSE(client_.uploader()->is_uploading());
}

TEST_F(ReportingServiceTest, ForceDiscard) {
  TestReportingService service(&client_, GetLocalState());
  service.AddLog(TestLog("log1"));

  service.EnableReporting();

  // Simulate the server returning a 500 error, which indicates that the server
  // is unhealthy.
  task_environment_.FastForwardBy(
      base::Seconds(MetricsScheduler::GetInitialIntervalSeconds()));
  EXPECT_TRUE(client_.uploader()->is_uploading());
  client_.uploader()->CompleteUpload(500);
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetInitialBackoffInterval());
  // Verify that the log is not discarded so that it can be re-sent later.
  EXPECT_TRUE(service.HasUnsentLogs());
  EXPECT_TRUE(client_.uploader()->is_uploading());

  // Simulate the server returning a 500 error again, but this time, with
  // |force_discard| set to true.
  client_.uploader()->CompleteUpload(500, /*force_discard=*/true);
  // Verify that the log was discarded, and that |service| is not uploading
  // anymore since there are no more logs.
  EXPECT_FALSE(service.HasUnsentLogs());
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0U);
  EXPECT_FALSE(client_.uploader()->is_uploading());
}

}  // namespace metrics
