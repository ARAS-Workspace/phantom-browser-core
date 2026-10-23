// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_CRASH_REPORTER_CLIENT_H_
#define COMPONENTS_CRASH_CORE_APP_CRASH_REPORTER_CLIENT_H_

#include <string>

#include "build/build_config.h"

namespace base {
class FilePath;
}

namespace crash_reporter {

struct ProductInfo {
  ProductInfo();
  ~ProductInfo();

  std::string product_name;
  std::string version;
  std::string channel;
};

class CrashReporterClient;

// Setter and getter for the client.  The client should be set early, before any
// crash reporter code is called, and should stay alive throughout the entire
// runtime.
void SetCrashReporterClient(CrashReporterClient* client);

#if defined(CRASH_IMPLEMENTATION)
// The components's embedder API should only be used by the component.
// WARNING: do not use this outside of the component.
CrashReporterClient* GetCrashReporterClient();
#endif

// Interface that the embedder implements.
class CrashReporterClient {
 public:
  // Type alias for subclasses outside of crash_reporter to reference
  // ProductInfo without needing to include the crash_reporter:: prefix.
  using ProductInfo = crash_reporter::ProductInfo;

  CrashReporterClient();
  virtual ~CrashReporterClient();

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  // Returns true if larger crash dumps should be dumped.
  virtual bool GetShouldDumpLargerDumps();
#endif

  // The location where minidump files should be written. Returns true if
  // |crash_dir| was set.
  virtual bool GetCrashDumpLocation(base::FilePath* crash_dir);

  // The location where metrics files should be written. Returns true if
  // |metrics_dir| was set.
  virtual bool GetCrashMetricsLocation(base::FilePath* metrics_dir);

  // Returns a textual description of the product info (product name, version,
  // etc.) to include in the crash report.
  virtual void GetProductInfo(ProductInfo* product_info);

  // Returns true if running in unattended mode (for automated testing).
  virtual bool IsRunningUnattended();

  // Returns true if the user has given consent to collect stats.
  virtual bool GetCollectStatsConsent();

  // Returns true if the client is currently in the chosen sample that will
  // report stats and crashes. Crashes should only be reported if this function
  // returns true and GetCollectStatsConsent returns true.
  virtual bool GetCollectStatsInSample();

  // Returns true if crash reporting is enforced via management policies. In
  // that case, |breakpad_enabled| is set to the value enforced by policies.
  virtual bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled);

  // Returns the URL target for crash report uploads.
  virtual std::string GetUploadUrl();
};

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_CRASH_REPORTER_CLIENT_H_
