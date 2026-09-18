// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

#include <memory>

#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_storage_partition.h"
#include "url/gurl.h"

class ChromeAutocompleteProviderClientTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    client_ = std::make_unique<ChromeAutocompleteProviderClient>(
        browser()->GetProfile());
    storage_partition_.set_service_worker_context(&service_worker_context_);
    client_->set_storage_partition(&storage_partition_);
  }

  void TearDownOnMainThread() override {
    client_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Replaces the client with one using an incognito profile. Note that this is
  // a one-way operation. Once a TEST_F calls this, all interactions with
  // |client_| will be off the record.
  void GoOffTheRecord() {
    client_ = std::make_unique<ChromeAutocompleteProviderClient>(
        browser()->GetProfile()->GetPrimaryOTRProfile(
            /*create_if_needed=*/true));
  }

  std::unique_ptr<ChromeAutocompleteProviderClient> client_;
  content::FakeServiceWorkerContext service_worker_context_;

 private:
  content::TestStoragePartition storage_partition_;
};

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       StartServiceWorker) {
  GURL destination_url("https://google.com/search?q=puppies");

  client_->StartServiceWorker(destination_url);
  EXPECT_TRUE(service_worker_context_
                  .start_service_worker_for_navigation_hint_called());
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       DontStartServiceWorkerInIncognito) {
  GURL destination_url("https://google.com/search?q=puppies");

  GoOffTheRecord();
  client_->StartServiceWorker(destination_url);
  EXPECT_FALSE(service_worker_context_
                   .start_service_worker_for_navigation_hint_called());
}

IN_PROC_BROWSER_TEST_F(ChromeAutocompleteProviderClientTest,
                       DontStartServiceWorkerIfSuggestDisabled) {
  GURL destination_url("https://google.com/search?q=puppies");

  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled,
                                                  false);
  client_->StartServiceWorker(destination_url);
  EXPECT_FALSE(service_worker_context_
                   .start_service_worker_for_navigation_hint_called());
}
