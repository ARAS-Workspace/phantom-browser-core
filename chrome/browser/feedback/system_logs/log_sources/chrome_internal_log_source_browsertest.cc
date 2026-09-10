// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/updater/browser_updater_client_testutils.h"  // nogncheck
#include "chrome/browser/updater/updater.h"
#include "chrome/updater/constants.h"       // nogncheck
#include "chrome/updater/update_service.h"  // nogncheck
#include "chrome/updater/updater_scope.h"   // nogncheck
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/test/base/scoped_channel_override.h"
#endif

namespace system_logs {
namespace {

std::unique_ptr<SystemLogsResponse> GetChromeInternalLogs() {
  base::RunLoop run_loop;
  ChromeInternalLogSource source;
  std::unique_ptr<SystemLogsResponse> response;
  source.Fetch(
      base::BindLambdaForTesting([&](std::unique_ptr<SystemLogsResponse> r) {
        response = std::move(r);
        run_loop.Quit();
      }));
  run_loop.Run();
  return response;
}

class ChromeInternalLogSourceTest : public InProcessBrowserTest {
 public:
  ChromeInternalLogSourceTest() = default;
  ChromeInternalLogSourceTest(const ChromeInternalLogSourceTest&) = delete;
  ChromeInternalLogSourceTest& operator=(const ChromeInternalLogSourceTest&) =
      delete;
  ~ChromeInternalLogSourceTest() override = default;
};

IN_PROC_BROWSER_TEST_F(ChromeInternalLogSourceTest,
                       VersionTagContainsActualVersion) {
  auto response = GetChromeInternalLogs();
  EXPECT_PRED_FORMAT2(
      testing::IsSubstring,
      chrome::GetVersionString(chrome::WithExtendedStable(true)),
      response->at("CHROME VERSION"));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(ChromeInternalLogSourceTest,
                       VersionTagContainsExtendedLabel) {
  chrome::ScopedChannelOverride channel_override(
      chrome::ScopedChannelOverride::Channel::kExtendedStable);

  ASSERT_TRUE(chrome::IsExtendedStableChannel());
  auto response = GetChromeInternalLogs();
  EXPECT_PRED_FORMAT2(
      testing::IsSubstring,
      chrome::GetVersionString(chrome::WithExtendedStable(true)),
      response->at("CHROME VERSION"));
}
#endif

IN_PROC_BROWSER_TEST_F(ChromeInternalLogSourceTest,
                       SkiaGraphiteStatusPresentAndValid) {
  content::GpuDataManager::GetInstance()->SetInitializedForTesting(false);
  auto response = GetChromeInternalLogs();
  auto value = response->at("skia_graphite_status");
  EXPECT_EQ(value, "unknown");

  content::GpuDataManager::GetInstance()->SetInitializedForTesting(true);
  content::GpuDataManager::GetInstance()->SetSkiaGraphiteEnabledForTesting(
      true);
  response = GetChromeInternalLogs();
  value = response->at("skia_graphite_status");
  EXPECT_EQ(value, "enabled");

  content::GpuDataManager::GetInstance()->SetSkiaGraphiteEnabledForTesting(
      false);
  response = GetChromeInternalLogs();
  value = response->at("skia_graphite_status");
  EXPECT_EQ(value, "disabled");
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ChromeInternalLogSourceTest, CpuTypePresentAndValid) {
  auto response = GetChromeInternalLogs();
  auto value = response->at("cpu_arch");
#if BUILDFLAG(IS_MAC)
  switch (base::mac::GetCPUType()) {
    case base::mac::CPUType::kIntel:
      EXPECT_EQ(value, "x86-64");
      break;
    case base::mac::CPUType::kTranslatedIntel:
      EXPECT_EQ(value, "x86-64/translated");
      break;
    case base::mac::CPUType::kArm:
      EXPECT_EQ(value, "arm64");
      break;
  }
#else
#if defined(ARCH_CPU_ARM64)
  EXPECT_EQ(value, "arm64");
#else
  bool emulated = base::win::OSInfo::IsRunningEmulatedOnArm64();
#if defined(ARCH_CPU_X86)
  if (emulated) {
    EXPECT_EQ(value, "32-bit emulated");
  } else {
    EXPECT_EQ(value, "32-bit");
  }
#else   // defined(ARCH_CPU_X86)
  if (emulated) {
    EXPECT_EQ(value, "64-bit emulated");
  } else {
    EXPECT_EQ(value, "64-bit");
  }
#endif  // defined(ARCH_CPU_X86)
#endif  // defined(ARCH_CPU_ARM64)
#endif
}
#endif

}  // namespace
}  // namespace system_logs
