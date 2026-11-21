// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/image_service_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/optimization_guide/core/hints/test_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_image_service/metrics_util.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_image_service {

class ImageServiceImplTest : public testing::Test {
 public:
  ImageServiceImplTest() = default;

  void SetUp() override {
    remote_suggestions_service_ = std::make_unique<RemoteSuggestionsService>(
        /*enterprise_search_aggregator_suggestions_service=*/nullptr,
        test_url_loader_factory_.GetSafeWeakWrapper());
    test_opt_guide_ =
        std::make_unique<optimization_guide::TestOptimizationGuideDecider>();
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    image_service_ = std::make_unique<ImageServiceImpl>(
        search_engines_test_environment_.template_url_service(),
        remote_suggestions_service_.get(), test_opt_guide_.get(),
        test_sync_service_.get(), std::make_unique<TestSchemeClassifier>());
  }

  PageImageServiceConsentStatus GetConsentStatusToFetchImageAwaitResult(
      mojom::ClientId client_id) {
    PageImageServiceConsentStatus out_status;
    base::RunLoop loop;
    image_service_->GetConsentToFetchImage(
        client_id,
        base::BindLambdaForTesting([&](PageImageServiceConsentStatus status) {
          out_status = status;
          loop.Quit();
        }));
    loop.Run();
    return out_status;
  }

  ImageServiceImplTest(const ImageServiceImplTest&) = delete;
  ImageServiceImplTest& operator=(const ImageServiceImplTest&) = delete;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  network::TestURLLoaderFactory test_url_loader_factory_;

  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<RemoteSuggestionsService> remote_suggestions_service_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideDecider>
      test_opt_guide_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<ImageServiceImpl> image_service_;

  base::HistogramTester histogram_tester_;
};

void AppendResponse(std::vector<GURL>* responses, const GURL& image_url) {
  DCHECK(responses);
  responses->push_back(image_url);
}

TEST_F(ImageServiceImplTest, DoesNotRegisterForNavigationRelatedMetadata) {
  ASSERT_EQ(test_opt_guide_->registered_optimization_types().size(), 0U);
}

TEST_F(ImageServiceImplTest, GetConsentToFetchImage) {
  test_sync_service_->SetDownloadStatusFor(
      {syncer::DataType::BOOKMARKS,
       syncer::DataType::HISTORY_DELETE_DIRECTIVES},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  test_sync_service_->FireStateChanged();

  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::Journeys),
            PageImageServiceConsentStatus::kTimedOut);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(
                mojom::ClientId::JourneysSidePanel),
            PageImageServiceConsentStatus::kTimedOut);
  EXPECT_EQ(
      GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::NtpRealbox),
      PageImageServiceConsentStatus::kFailure);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::NtpQuests),
            PageImageServiceConsentStatus::kTimedOut);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::Bookmarks),
            PageImageServiceConsentStatus::kTimedOut);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(
                mojom::ClientId::NtpTabResumption),
            PageImageServiceConsentStatus::kTimedOut);

  test_sync_service_->SetDownloadStatusFor(
      {syncer::DataType::HISTORY_DELETE_DIRECTIVES},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service_->FireStateChanged();

  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::Journeys),
            PageImageServiceConsentStatus::kSuccess);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(
                mojom::ClientId::JourneysSidePanel),
            PageImageServiceConsentStatus::kSuccess);
  // NTP Realbox still false as it does not have an approved privacy model yet.
  EXPECT_EQ(
      GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::NtpRealbox),
      PageImageServiceConsentStatus::kFailure);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::NtpQuests),
            PageImageServiceConsentStatus::kSuccess);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::Bookmarks),
            PageImageServiceConsentStatus::kTimedOut);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(
                mojom::ClientId::NtpTabResumption),
            PageImageServiceConsentStatus::kSuccess);
}

TEST_F(ImageServiceImplTest, SyncInitialization) {
  // Put Sync into the initializing state.
  test_sync_service_->SetDownloadStatusFor(
      {syncer::DataType::BOOKMARKS,
       syncer::DataType::HISTORY_DELETE_DIRECTIVES},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  test_sync_service_->FireStateChanged();

  mojom::Options options;
  options.suggest_images = false;
  options.optimization_guide_images = true;

  // Requests that pass the consent throttle record a backend selection.
  auto backend_requests = [&] {
    return histogram_tester_.GetBucketCount(
        "PageImageService.Backend", PageImageServiceBackend::kNoValidBackend);
  };

  std::vector<GURL> responses;
  image_service_->FetchImageFor(mojom::ClientId::Journeys,
                                GURL("https://page-url.com"), options,
                                base::BindOnce(&AppendResponse, &responses));
  EXPECT_EQ(backend_requests(), 0)
      << "Expect no immediate requests, because the consent should be "
         "throttling it.";
  EXPECT_TRUE(responses.empty());

  task_environment.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(backend_requests(), 0)
      << "After 10 seconds, the throttle should have killed the request, never "
         "passing it to the backend.";
  ASSERT_EQ(responses.size(), 1U);
  EXPECT_EQ(responses[0], GURL());

  // Now send another request.
  image_service_->FetchImageFor(mojom::ClientId::Journeys,
                                GURL("https://page-url.com"), options,
                                base::BindOnce(&AppendResponse, &responses));
  task_environment.FastForwardBy(base::Seconds(3));
  EXPECT_EQ(backend_requests(), 0) << "Still throttled.";

  // Now set the test sync service to active.
  test_sync_service_->SetDownloadStatusFor(
      {syncer::DataType::BOOKMARKS,
       syncer::DataType::HISTORY_DELETE_DIRECTIVES},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service_->FireStateChanged();
  EXPECT_EQ(backend_requests(), 1)
      << "The request should immediately reach backend selection after Sync "
         "activates and the consent throttle unthrottles.";

  // This test only covers sync unthrottling.
}

}  // namespace page_image_service
