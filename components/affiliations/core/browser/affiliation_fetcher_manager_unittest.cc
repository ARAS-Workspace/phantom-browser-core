// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_fetcher_manager.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/affiliations/core/browser/affiliation_api.pb.h"
#include "components/affiliations/core/browser/affiliation_fetcher_factory_impl.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "components/affiliations/core/browser/fake_affiliation_api.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace affiliations {

namespace {
constexpr char kNotExampleAndroidFacetURI[] =
    "android://hash1234@com.example.not";
constexpr char kExampleWebFacet1URI[] = "https://www.example.com";
constexpr AffiliationFetcherInterface::RequestInfo kRequestInfo{
    .branding_info = true,
    .change_password_info = true};

// Fetcher factory that mirrors the production factory: it creates no fetcher.
// The bool lets the tests flip creation reporting on and off.
class KeylessFetcherFactory : public AffiliationFetcherFactory {
 public:
  KeylessFetcherFactory();
  ~KeylessFetcherFactory() override;

  std::unique_ptr<AffiliationFetcherInterface> CreateInstance(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  void SetCanCreateFetcher(bool can_create_fetcher) {
    can_create_fetcher_ = can_create_fetcher;
  }

  bool CanCreateFetcher() const override { return can_create_fetcher_; }

 private:
  bool can_create_fetcher_ = true;
};

KeylessFetcherFactory::KeylessFetcherFactory() = default;
KeylessFetcherFactory::~KeylessFetcherFactory() = default;

std::unique_ptr<AffiliationFetcherInterface>
KeylessFetcherFactory::CreateInstance(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return nullptr;
}
}  // namespace

class AffiliationFetcherManagerTest : public testing::Test {
 public:
  AffiliationFetcherManagerTest() = default;

 protected:

  int GetNumPendingRequests() { return test_url_loader_factory_.NumPending(); }

  AffiliationFetcherManager* manager() { return manager_.get(); }

  void DisallowFetcherCreation() {
    keyless_fetcher_factory_->SetCanCreateFetcher(false);
  }

  void DestroyManager() {
    keyless_fetcher_factory_ = nullptr;
    manager_.reset();
  }

 private:
  // testing::Test:
  void SetUp() override {
    manager_ = std::make_unique<AffiliationFetcherManager>(
        test_shared_loader_factory_);
    auto fetcher_factory = std::make_unique<KeylessFetcherFactory>();
    keyless_fetcher_factory_ = fetcher_factory.get();

    manager_->SetFetcherFactoryForTesting(std::move(fetcher_factory));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  std::unique_ptr<AffiliationFetcherManager> manager_;
  // Owned by |manager_|.
  raw_ptr<KeylessFetcherFactory> keyless_fetcher_factory_ = nullptr;
};

TEST_F(AffiliationFetcherManagerTest,
       ImmediatelyInvokeCallbackIfFetcherCreationFailed) {
  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));
  base::test::TestFuture<AffiliationFetcherInterface::FetchResult>
      completion_callback;
  DisallowFetcherCreation();

  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback.GetCallback());

  EXPECT_FALSE(manager()->IsFetchPossible());
  EXPECT_EQ(0u, manager()->GetFetchersForTesting()->size());
  EXPECT_EQ(0, GetNumPendingRequests());
  EXPECT_TRUE(completion_callback.IsReady());
}
}  // namespace affiliations
