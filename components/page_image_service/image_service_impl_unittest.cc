// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/image_service_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/test_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/salient_image_metadata.pb.h"
#include "components/page_image_service/metrics_util.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace optimization_guide {
namespace {

}  // namespace
}  // namespace optimization_guide

namespace page_image_service {

class ImageServiceImplTest : public testing::Test {
 public:
  ImageServiceImplTest() = default;

  void SetUp() override {
    remote_suggestions_service_ = std::make_unique<RemoteSuggestionsService>(
        /*document_suggestions_service=*/nullptr,
        /*enterprise_search_aggregator_suggestions_service=*/nullptr,
        test_url_loader_factory_.GetSafeWeakWrapper());
    test_opt_guide_ =
        std::make_unique<optimization_guide::ImageServiceTestOptGuide>();
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
  std::unique_ptr<optimization_guide::ImageServiceTestOptGuide> test_opt_guide_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<ImageServiceImpl> image_service_;

  base::HistogramTester histogram_tester_;
};

// Helper method that stores `image_url` into `out_image_url`.
void StoreImageUrlResponse(GURL* out_image_url, const GURL& image_url) {
  DCHECK(out_image_url);
  *out_image_url = image_url;
}

// Stores an image response and exits out of `loop` if it is defined.
void QuitLoopAndStoreImageUrlResponse(base::RunLoop* loop,
                                      GURL* out_image_url,
                                      const GURL& image_url) {
  DCHECK(out_image_url);
  *out_image_url = image_url;
  loop->Quit();
}

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

  std::vector<GURL> responses;
  image_service_->FetchImageFor(mojom::ClientId::Journeys,
                                GURL("https://page-url.com"), options,
                                base::BindOnce(&AppendResponse, &responses));
  EXPECT_EQ(test_opt_guide_->requests_received_, 0U)
      << "Expect no immediate requests, because the consent should be "
         "throttling it.";
  EXPECT_TRUE(responses.empty());

  task_environment.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(test_opt_guide_->requests_received_, 0U)
      << "After 10 seconds, the throttle should have killed the request, never "
         "passing it to the backend.";
  ASSERT_EQ(responses.size(), 1U);
  EXPECT_EQ(responses[0], GURL());

  // Now send another request.
  image_service_->FetchImageFor(mojom::ClientId::Journeys,
                                GURL("https://page-url.com"), options,
                                base::BindOnce(&AppendResponse, &responses));
  task_environment.FastForwardBy(base::Seconds(3));
  EXPECT_EQ(test_opt_guide_->requests_received_, 0U) << "Still throttled.";

  // Now set the test sync service to active.
  test_sync_service_->SetDownloadStatusFor(
      {syncer::DataType::BOOKMARKS,
       syncer::DataType::HISTORY_DELETE_DIRECTIVES},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service_->FireStateChanged();
  task_environment.FastForwardBy(kOptimizationGuideBatchingTimeout);
  EXPECT_EQ(test_opt_guide_->requests_received_, 1U)
      << "The test backend should immediately get the request after Sync "
         "activates, and the consent throttle unthrottles, and after the "
         "short aggregation timeout expires.";

  // This test only covers sync unthrottling, so we don't care about fulfilling
  // the actual request. That's covered by
  // OptimizationGuideSalientImagesEndToEnd.
}

TEST_F(ImageServiceImplTest, SuggestBackendEndToEnd) {
  mojom::Options options;
  options.suggest_images = true;
  options.optimization_guide_images = true;

  base::RunLoop loop;

  GURL response;
  image_service_->FetchImageFor(
      mojom::ClientId::Journeys,
      GURL("https://www.google.com/search?q=santa+monica"), options,
      base::BindOnce(&QuitLoopAndStoreImageUrlResponse, &loop, &response));

  // Test histograms with literal names to validate client-sliced names.
  // This also validates that the correct backend was selected.
  EXPECT_EQ(histogram_tester_.GetBucketCount("PageImageService.Backend",
                                             PageImageServiceBackend::kSuggest),
            1);
  EXPECT_EQ(
      histogram_tester_.GetBucketCount("PageImageService.Backend.Journeys",
                                       PageImageServiceBackend::kSuggest),
      1);

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  GURL request_url = test_url_loader_factory_.GetPendingRequest(0)->request.url;
  EXPECT_EQ(request_url.GetHost(), "www.google.com");

  test_url_loader_factory_.AddResponse(request_url.spec(), R"([
  "santa monica",
  [
    "santa monica"
  ],
  [
    ""
  ],
  [],
  {
    "google:clientdata": {
      "bpc": false,
      "tlw": false
    },
    "google:suggestdetail": [
      {
        "google:entityinfo": "CggvbS8wNl9raBISQ2l0eSBpbiBDYWxpZm9ybmlhMnRodHRwczovL2VuY3J5cHRlZC10Ym4wLmdzdGF0aWMuY29tL2ltYWdlcz9xPXRibjpBTmQ5R2NTd3ZOaHc3cktRV2dqRG9vUC1zY1ptRHlTSlNJWWpCT1gwVkVDRDU1czM4dHA0eEZORWcwTTdQdUEmcz0xMDoMU2FudGEgTW9uaWNhSgcjNDI0MjQyUjVnc19zc3A9ZUp6ajR0RFAxVGN3aThfT01HRDA0aWxPekN0SlZNak56OHRNVGdRQVdDY0hxd3AMcBo="
      }
    ],
    "google:suggestrelevance": [
      1300
    ],
    "google:suggestsubtypes": [
      [
        131,
        433,
        512
      ]
    ],
    "google:suggesttype": [
      "ENTITY"
    ],
    "google:verbatimrelevance": 1300
  }
])");

  // Successfully fetching the image quits this loop.
  loop.Run();
  // This expected value matches the hardcoded proto above.
  EXPECT_EQ(response, GURL("https://encrypted-tbn0.gstatic.com/"
                           "images?q=tbn:ANd9GcSwvNhw7rKQWgjDooP-"
                           "scZmDySJSIYjBOX0VECD55s38tp4xFNEg0M7PuA&s=10"));

  // Test histograms with literal names to validate client-sliced names.
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "PageImageService.Backend.Suggest.Result",
                PageImageServiceResult::kSuccess),
            1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "PageImageService.Backend.Suggest.Result.Journeys",
                PageImageServiceResult::kSuccess),
            1);
}

// This also tests batching, because it's an integral part of how Optimization
// Guide backend works.
}  // namespace page_image_service
