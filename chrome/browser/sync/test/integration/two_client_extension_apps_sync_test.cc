// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/sync/extension_sync_data.h"
#include "chrome/browser/extensions/sync/extension_sync_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/apps_sync_test_base.h"
#include "chrome/browser/sync/test/integration/sync_app_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/sync/base/features.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/launch_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/constants.h"

using apps_helper::AllProfilesHaveSameApps;
using apps_helper::DisableApp;
using apps_helper::EnableApp;
using apps_helper::GetAppLaunchOrdinalForApp;
using apps_helper::IncognitoDisableApp;
using apps_helper::IncognitoEnableApp;
using apps_helper::InstallHostedApp;
using apps_helper::InstallPlatformApp;
using apps_helper::SetAppLaunchOrdinalForApp;
using apps_helper::SetPageOrdinalForApp;
using apps_helper::UninstallApp;

namespace {

extensions::ExtensionRegistry* GetExtensionRegistry(Profile* profile) {
  return extensions::ExtensionRegistry::Get(profile);
}

}  // namespace

class TwoClientExtensionAppsSyncTest : public AppsSyncTestBase {
 public:
  TwoClientExtensionAppsSyncTest() : AppsSyncTestBase(TWO_CLIENT) {}

  TwoClientExtensionAppsSyncTest(const TwoClientExtensionAppsSyncTest&) =
      delete;
  TwoClientExtensionAppsSyncTest& operator=(
      const TwoClientExtensionAppsSyncTest&) = delete;

  ~TwoClientExtensionAppsSyncTest() override = default;

  // Apps sync is only supported with Sync-the-feature.
  SetupSyncMode GetSetupSyncMode() const override {
    return SetupSyncMode::kSyncTheFeature;
  }
};

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest,
                       UninstallOnWML) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupClients());

  auto appid1 = InstallHostedApp(GetProfile(0), 0);
  auto appid2 = InstallHostedApp(GetProfile(1), 0);
  EXPECT_EQ(appid1, appid2);

  int i = 1;

  const int kNumCommonApps = 4;
  for (int j = 0; j < kNumCommonApps; ++i, ++j) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());
  const int kNumProfile0Apps = 10;
  for (int j = 0; j < kNumProfile0Apps; ++i, ++j) {
    InstallHostedApp(GetProfile(0), i);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    InstallHostedApp(GetProfile(1), i);
  }

  ASSERT_TRUE(AwaitQuiescence());

  // Chrome App installs via sync are disabled on WML.
  ASSERT_FALSE(apps_helper::HasSameApps(GetProfile(0), GetProfile(1)));

  ASSERT_TRUE(
      GetExtensionRegistry(GetProfile(0))->GetInstalledExtension(appid1));
  ASSERT_TRUE(
      GetExtensionRegistry(GetProfile(1))->GetInstalledExtension(appid2));

  UninstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AwaitQuiescence());

  // Uninstalls are still sync-ed and applied on WML devices.
  ASSERT_FALSE(
      GetExtensionRegistry(GetProfile(0))->GetInstalledExtension(appid1));
  ASSERT_FALSE(
      GetExtensionRegistry(GetProfile(1))->GetInstalledExtension(appid1));
}

#endif

// TODO(akalin): Add tests exercising:
//   - Offline installation/uninstallation behavior
//   - App-specific properties
