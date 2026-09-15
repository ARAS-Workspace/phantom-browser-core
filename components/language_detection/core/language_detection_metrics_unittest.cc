// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/language_detection_metrics.h"

#include <memory>
#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::HistogramBase;
using base::HistogramSamples;
using base::StatisticsRecorder;

namespace language_detection {
namespace {

class MetricsRecorder {
 public:
  explicit MetricsRecorder(const char* key) : key_(key) {
    HistogramBase* histogram = StatisticsRecorder::FindHistogram(key_);
    if (histogram) {
      base_samples_ = histogram->SnapshotSamples();
    }
  }

  MetricsRecorder(const MetricsRecorder&) = delete;
  MetricsRecorder& operator=(const MetricsRecorder&) = delete;

  void CheckLanguageVerification(int expected_model_only,
                                 int expected_unknown,
                                 int expected_model_agree,
                                 int expected_model_disagree,
                                 int expected_trust_model,
                                 int expected_model_complement_sub_code,
                                 int expected_no_page_content,
                                 int expected_model_not_available) {
    ASSERT_EQ(metrics_internal::kLanguageDetectionLanguageVerification, key_);

    Snapshot();

    // EXPECT_EQ(expected_model_disabled,
    //           GetCountWithoutSnapshot(kModelDisabled)); -- obsolete
    EXPECT_EQ(expected_model_only, GetCountWithoutSnapshot(static_cast<int>(
                                       LanguageVerificationType::kModelOnly)));
    EXPECT_EQ(expected_unknown, GetCountWithoutSnapshot(static_cast<int>(
                                    LanguageVerificationType::kModelUnknown)));
    EXPECT_EQ(expected_model_agree,
              GetCountWithoutSnapshot(
                  static_cast<int>(LanguageVerificationType::kModelAgrees)));
    EXPECT_EQ(expected_model_disagree,
              GetCountWithoutSnapshot(
                  static_cast<int>(LanguageVerificationType::kModelDisagrees)));
    EXPECT_EQ(expected_trust_model,
              GetCountWithoutSnapshot(
                  static_cast<int>(LanguageVerificationType::kModelOverrides)));
    EXPECT_EQ(expected_model_complement_sub_code,
              GetCountWithoutSnapshot(static_cast<int>(
                  LanguageVerificationType::kModelComplementsCountry)));
    EXPECT_EQ(expected_no_page_content,
              GetCountWithoutSnapshot(
                  static_cast<int>(LanguageVerificationType::kNoPageContent)));
    EXPECT_EQ(expected_model_not_available,
              GetCountWithoutSnapshot(static_cast<int>(
                  LanguageVerificationType::kModelNotAvailable)));
  }

 private:
  void Snapshot() {
    HistogramBase* histogram = StatisticsRecorder::FindHistogram(key_);
    if (!histogram) {
      return;
    }
    samples_ = histogram->SnapshotSamples();
  }

  HistogramBase::Count32 GetCountWithoutSnapshot(
      HistogramBase::Sample32 value) {
    if (!samples_) {
      return 0;
    }
    HistogramBase::Count32 count = samples_->GetCount(value);
    if (!base_samples_) {
      return count;
    }
    return count - base_samples_->GetCount(value);
  }

  std::string key_;
  std::unique_ptr<HistogramSamples> base_samples_;
  std::unique_ptr<HistogramSamples> samples_;
};

TEST(LanguageDetectionMetricsTest, ReportLanguageVerification) {
  MetricsRecorder recorder(
      metrics_internal::kLanguageDetectionLanguageVerification);

  // ReportLanguageVerification(kModelDisabled); -- obsolete
  recorder.CheckLanguageVerification(0, 0, 0, 0, 0, 0, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelOnly);
  recorder.CheckLanguageVerification(1, 0, 0, 0, 0, 0, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelUnknown);
  recorder.CheckLanguageVerification(1, 1, 0, 0, 0, 0, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelAgrees);
  recorder.CheckLanguageVerification(1, 1, 1, 0, 0, 0, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelDisagrees);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 0, 0, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelOverrides);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 0, 0, 0);
  ReportLanguageVerification(
      LanguageVerificationType::kModelComplementsCountry);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kNoPageContent);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 1, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelNotAvailable);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 1, 1);
}

}  // namespace
}  // namespace language_detection
