// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_fetcher_factory_impl.h"

#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace affiliations {

AffiliationFetcherFactoryImpl::AffiliationFetcherFactoryImpl() = default;
AffiliationFetcherFactoryImpl::~AffiliationFetcherFactoryImpl() = default;

std::unique_ptr<AffiliationFetcherInterface>
AffiliationFetcherFactoryImpl::CreateInstance(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return nullptr;
}

bool AffiliationFetcherFactoryImpl::CanCreateFetcher() const {
  return false;
}

}  // namespace affiliations
