// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_downloader_factory.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/network.h"
#include "components/update_client/url_fetcher_downloader.h"

namespace update_client {
namespace {

class CrxDownloaderFactoryChromium : public CrxDownloaderFactory {
 public:
  explicit CrxDownloaderFactoryChromium(
      scoped_refptr<NetworkFetcherFactory> network_fetcher_factory)
      : network_fetcher_factory_(network_fetcher_factory) {}

  // Overrides for CrxDownloaderFactory.
  scoped_refptr<CrxDownloader> MakeCrxDownloader(
      const std::string& prod_id) const override;

 private:
  ~CrxDownloaderFactoryChromium() override = default;

  scoped_refptr<NetworkFetcherFactory> network_fetcher_factory_;
};

scoped_refptr<CrxDownloader> CrxDownloaderFactoryChromium::MakeCrxDownloader(
    const std::string& prod_id) const {
  scoped_refptr<CrxDownloader> url_fetcher_downloader =
      base::MakeRefCounted<UrlFetcherDownloader>(
          nullptr, network_fetcher_factory_, prod_id);

  return url_fetcher_downloader;
}

}  // namespace

scoped_refptr<CrxDownloaderFactory> MakeCrxDownloaderFactory(
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory) {
  return base::MakeRefCounted<CrxDownloaderFactoryChromium>(
      network_fetcher_factory);
}

}  // namespace update_client
