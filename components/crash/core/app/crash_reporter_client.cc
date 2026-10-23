// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/crash_reporter_client.h"

#include "build/build_config.h"

#include "base/check.h"

namespace crash_reporter {

namespace {

CrashReporterClient* g_client = nullptr;

}  // namespace

ProductInfo::ProductInfo() = default;
ProductInfo::~ProductInfo() = default;

void SetCrashReporterClient(CrashReporterClient* client) {
  g_client = client;
}

CrashReporterClient* GetCrashReporterClient() {
  DCHECK(g_client);
  return g_client;
}

CrashReporterClient::CrashReporterClient() = default;
CrashReporterClient::~CrashReporterClient() = default;

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
bool CrashReporterClient::GetShouldDumpLargerDumps() {
  return false;
}
#endif

bool CrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
  return false;
}

bool CrashReporterClient::GetCrashMetricsLocation(base::FilePath* crash_dir) {
  return false;
}

void CrashReporterClient::GetProductInfo(ProductInfo* product_info) {}

bool CrashReporterClient::IsRunningUnattended() {
  return true;
}

bool CrashReporterClient::GetCollectStatsConsent() {
  return false;
}

bool CrashReporterClient::GetCollectStatsInSample() {
  // By default, clients don't do sampling, so everything will be "in-sample".
  return true;
}

bool CrashReporterClient::ReportingIsEnforcedByPolicy(bool* breakpad_enabled) {
  return false;
}

std::string CrashReporterClient::GetUploadUrl() {
  return std::string();
}

}  // namespace crash_reporter
