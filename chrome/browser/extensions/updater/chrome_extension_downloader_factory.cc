// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/chrome_extension_downloader_factory.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "chrome/browser/extensions/updater/extension_updater_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/verifier_formats.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using extensions::ExtensionDownloader;
using extensions::ExtensionDownloaderDelegate;
using update_client::UpdateQueryParams;

std::unique_ptr<ExtensionDownloader>
ChromeExtensionDownloaderFactory::CreateForURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ExtensionDownloaderDelegate* delegate,
    crx_file::VerifierFormat required_verifier_format,
    const base::FilePath& profile_path) {
  std::unique_ptr<ExtensionDownloader> downloader(
      new ExtensionDownloader(delegate, std::move(url_loader_factory),
                              required_verifier_format, profile_path));
  std::string manifest_query_params =
      UpdateQueryParams::Get(UpdateQueryParams::CRX);
  base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(extensions::kSwitchTestRequestParam)) {
    manifest_query_params += "&testrequest=1";
  }
  downloader->set_manifest_query_params(manifest_query_params);
  return downloader;
}

std::unique_ptr<ExtensionDownloader>
ChromeExtensionDownloaderFactory::CreateForProfile(
    Profile* profile,
    ExtensionDownloaderDelegate* delegate) {
  return CreateForURLLoaderFactory(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      delegate, extensions::GetPolicyVerifierFormat(), profile->GetPath());
}
