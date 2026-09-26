// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_content_browser_client.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"

namespace content {

// static
TestContentBrowserClient* TestContentBrowserClient::instance_ = nullptr;

TestContentBrowserClient::TestContentBrowserClient() {
  instance_ = this;
}

TestContentBrowserClient::~TestContentBrowserClient() {
  if (instance_ == this) {
    instance_ = nullptr;
  }
}

// static
TestContentBrowserClient* TestContentBrowserClient::GetInstance() {
  return instance_;
}

base::FilePath TestContentBrowserClient::GetDefaultDownloadDirectory() {
  if (!download_dir_.IsValid()) {
    bool result = download_dir_.CreateUniqueTempDir();
    CHECK(result);
  }
  return download_dir_.GetPath();
}

GeneratedCodeCacheSettings
TestContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return GeneratedCodeCacheSettings(true, 0, context->GetPath());
}

std::string TestContentBrowserClient::GetUserAgent() {
  return std::string("TestContentClient");
}

std::string TestContentBrowserClient::GetApplicationLocale() {
  return application_locale_.empty()
             ? ContentBrowserClient::GetApplicationLocale()
             : application_locale_;
}

}  // namespace content
