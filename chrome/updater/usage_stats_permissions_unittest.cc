// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/usage_stats_permissions.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

#if BUILDFLAG(IS_MAC)
#include "base/files/scoped_temp_dir.h"
#include "chrome/updater/util/mac_util.h"
#endif

namespace updater {

class UsageStatsPermissionsTest : public testing::Test {
 protected:
  EventLoggingPermissionProvider fake_permission_provider_ = {
      .app_id = "UsageStatsTestPermissionProvider",
#if BUILDFLAG(IS_MAC)
      .directory_name = "UsageStatsTestPermissionProvider",
#endif
  };

#if BUILDFLAG(IS_MAC)
  base::ScopedTempDir fake_user_directory_;
  base::ScopedTempDir fake_system_directory_;

  void SetAppUsageStats(const std::string& app_id,
                        bool enabled,
                        UpdaterScope scope) {
    base::FilePath app_support_dir = IsSystemInstall(scope)
                                         ? fake_system_directory_.GetPath()
                                         : fake_user_directory_.GetPath();
    base::FilePath app_dir =
        app_support_dir.Append(COMPANY_SHORTNAME_STRING).Append(app_id);

    ASSERT_TRUE(base::CreateDirectory(app_dir));
    std::unique_ptr<crashpad::CrashReportDatabase> database =
        crashpad::CrashReportDatabase::Initialize(app_dir.Append("Crashpad"));
    ASSERT_TRUE(database &&
                database->GetSettings()->SetUploadsEnabled(enabled));
    installed_app_ids_.push_back(app_id);
  }

  void SetUp() override {
    ASSERT_TRUE(fake_user_directory_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_system_directory_.CreateUniqueTempDir());
  }

  bool AnyAppEnablesUsageStats() {
    return ::updater::AnyAppEnablesUsageStats(ApplicationSupportDirectories());
  }

  bool RemoteEventLoggingAllowed() {
    return ::updater::RemoteEventLoggingAllowed(installed_app_ids_,
                                                ApplicationSupportDirectories(),
                                                fake_permission_provider_);
  }

#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC)
  void SetExemptAppsUsageStats(bool enabled, UpdaterScope scope) {
    SetAppUsageStats(kUpdaterAppId, enabled, scope);
    SetAppUsageStats(enterprise_companion::kCompanionAppId, enabled, scope);
    SetAppUsageStats(kPlatformExperienceHelperAppId, enabled, scope);
  }
#endif  // BUILDFLAG(IS_MAC)

  UpdaterScope scope_ = GetUpdaterScopeForTesting();

 private:
#if BUILDFLAG(IS_MAC)
  std::vector<base::FilePath> ApplicationSupportDirectories() {
    std::vector<base::FilePath> application_support_directories(
        {fake_user_directory_.GetPath()});
    if (IsSystemInstall(scope_)) {
      application_support_directories.push_back(
          fake_system_directory_.GetPath());
    }
    return application_support_directories;
  }
#endif

  std::vector<std::string> installed_app_ids_;
};

#if BUILDFLAG(IS_LINUX)
TEST_F(UsageStatsPermissionsTest, LinuxAlwaysFalse) {
  ASSERT_FALSE(AnyAppEnablesUsageStats(scope_));
  ASSERT_FALSE(
      RemoteEventLoggingAllowed(scope_, {}, fake_permission_provider_));
}
#else

TEST_F(UsageStatsPermissionsTest, NoApps) {
  ASSERT_FALSE(AnyAppEnablesUsageStats());
}

TEST_F(UsageStatsPermissionsTest, OneAppDisabled) {
  SetAppUsageStats("app1", false, scope_);
  SetAppUsageStats("app2", false, scope_);
  ASSERT_FALSE(AnyAppEnablesUsageStats());
}

TEST_F(UsageStatsPermissionsTest, OneAppEnabled) {
  SetAppUsageStats("app1", true, scope_);
  SetAppUsageStats("app2", false, scope_);
  ASSERT_TRUE(AnyAppEnablesUsageStats());
}

TEST_F(UsageStatsPermissionsTest, UserInstallIgnoresSystem) {
  if (IsSystemInstall(scope_)) {
    GTEST_SKIP() << "Not applicable to system-scoped installs";
  }
  SetAppUsageStats("app1", false, UpdaterScope::kUser);
  SetAppUsageStats("app1", true, UpdaterScope::kSystem);
  ASSERT_FALSE(AnyAppEnablesUsageStats());
}

TEST_F(UsageStatsPermissionsTest, SystemInstallLooksAtUser) {
  if (!IsSystemInstall(scope_)) {
    GTEST_SKIP() << "Not applicable to user-scoped installs";
  }
  SetAppUsageStats("app1", true, UpdaterScope::kUser);
  SetAppUsageStats("app1", false, UpdaterScope::kSystem);
  ASSERT_TRUE(AnyAppEnablesUsageStats());
}

TEST_F(UsageStatsPermissionsTest, PermissionProviderAllowsRemoteLogging) {
  SetAppUsageStats(fake_permission_provider_.app_id, true, scope_);
  ASSERT_TRUE(RemoteEventLoggingAllowed());
}

TEST_F(UsageStatsPermissionsTest,
       PermissionProviderAllowsRemoteLoggingWithExemptApps) {
  SetExemptAppsUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_.app_id, true, scope_);
  ASSERT_TRUE(RemoteEventLoggingAllowed());
}

TEST_F(UsageStatsPermissionsTest, UsageStatsProviderChecksPermissionProvider) {
  SetExemptAppsUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_.app_id, false, scope_);
  ASSERT_FALSE(RemoteEventLoggingAllowed());
}

TEST_F(UsageStatsPermissionsTest,
       PermissionProviderDisallowsRemoteLoggingWithOtherAppDisabled) {
  SetExemptAppsUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_.app_id, true, scope_);
  SetAppUsageStats("unsupported_app", false, scope_);
  ASSERT_FALSE(RemoteEventLoggingAllowed());
}

TEST_F(UsageStatsPermissionsTest,
       PermissionProviderDisallowsRemoteLoggingWithOtherAppEnabled) {
  SetExemptAppsUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_.app_id, true, scope_);
  SetAppUsageStats("unsupported_app", true, scope_);
  ASSERT_FALSE(RemoteEventLoggingAllowed());
}

TEST_F(UsageStatsPermissionsTest,
       SystemPermissionProviderAllowsRemoteLoggingWithUserAppEnabled) {
  if (!IsSystemInstall(scope_)) {
    GTEST_SKIP() << "Not applicable to user-scoped installs";
  }
  SetExemptAppsUsageStats(true, scope_);
  SetAppUsageStats(fake_permission_provider_.app_id, true, UpdaterScope::kUser);
  SetAppUsageStats(fake_permission_provider_.app_id, false,
                   UpdaterScope::kSystem);
  ASSERT_TRUE(RemoteEventLoggingAllowed());
}

#endif

}  // namespace updater
