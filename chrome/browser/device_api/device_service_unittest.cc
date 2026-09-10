// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/device_api/device_attribute_api.h"
#include "chrome/browser/device_api/device_service_impl.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_test_server_configurator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/webapps/isolated_web_apps/scheme.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace {

constexpr char kDefaultAppInstallUrl[] = "https://example.com/install";
constexpr char kTrustedUrl[] = "https://example.com/sample";
constexpr char kUntrustedUrl[] = "https://non-example.com/sample";
constexpr char kUserEmail[] = "user-email@example.com";


}  // namespace

namespace {


constexpr char kAnnotatedAssetId[] = "annotated_asset_id";
constexpr char kAnnotatedLocation[] = "annotated_location";
constexpr char kDirectoryApiId[] = "directory_api_id";
constexpr char kHostname[] = "hostname";
constexpr char kSerialNumber[] = "serial_number";

class FakeDeviceAttributeApi : public DeviceAttributeApi {
 public:
  FakeDeviceAttributeApi() = default;
  ~FakeDeviceAttributeApi() override = default;

  // This method forwards calls to DeviceAttributesApiImpl to the test the
  // actual error reported by the service.
  void ReportNotAllowedError(NotificationCallback callback) override {
    device_attributes_api_.ReportNotAllowedError(std::move(callback));
  }

  // This method forwards calls to DeviceAttributesApiImpl to the test the
  // actual error reported by the service.
  void ReportNotAffiliatedError(NotificationCallback callback) override {
    device_attributes_api_.ReportNotAffiliatedError(std::move(callback));
  }

  void GetDirectoryId(blink::mojom::DeviceAPIService::GetDirectoryIdCallback
                          callback) override {
    std::move(callback).Run(
        blink::mojom::DeviceAttributeValue::New(kDirectoryApiId));
  }

  void GetHostname(
      blink::mojom::DeviceAPIService::GetHostnameCallback callback) override {
    std::move(callback).Run(blink::mojom::DeviceAttributeValue::New(kHostname));
  }

  void GetSerialNumber(blink::mojom::DeviceAPIService::GetSerialNumberCallback
                           callback) override {
    std::move(callback).Run(
        blink::mojom::DeviceAttributeValue::New(kSerialNumber));
  }

  void GetAnnotatedAssetId(
      blink::mojom::DeviceAPIService::GetAnnotatedAssetIdCallback callback)
      override {
    std::move(callback).Run(
        blink::mojom::DeviceAttributeValue::New(kAnnotatedAssetId));
  }

  void GetAnnotatedLocation(
      blink::mojom::DeviceAPIService::GetAnnotatedLocationCallback callback)
      override {
    std::move(callback).Run(
        blink::mojom::DeviceAttributeValue::New(kAnnotatedLocation));
  }

 private:
  DeviceAttributeApiImpl device_attributes_api_;
};
}  // namespace

class DeviceAPIServiceTest {
 public:
  void InstallTrustedApps(Profile* profile) {
    app_id_ = web_app::test::InstallDummyWebApp(
        profile, "Policy installed app", GURL(kDefaultAppInstallUrl),
        webapps::WebappInstallSource::EXTERNAL_POLICY);
  }

  void TryCreatingService(
      const GURL& url,
      std::unique_ptr<DeviceAttributeApi> device_attribute_api,
      content::WebContents* web_contents) {
    // Isolated Web Apps require Cross Origin Isolation headers to be included
    // in the response.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    if (url.SchemeIs(webapps::kIsolatedAppScheme)) {
      web_app::SimulateIsolatedWebAppNavigation(web_contents, url);
    } else {
      content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents,
                                                                 url);
    }
#else
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents,
                                                               url);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

    DeviceServiceImpl::CreateForTest(web_contents->GetPrimaryMainFrame(),
                                     remote()->BindNewPipeAndPassReceiver(),
                                     std::move(device_attribute_api));
  }

  const webapps::AppId& app_id() const { return *app_id_; }

  mojo::Remote<blink::mojom::DeviceAPIService>* remote() { return &remote_; }

 private:
  std::optional<webapps::AppId> app_id_;
  mojo::Remote<blink::mojom::DeviceAPIService> remote_;
};

namespace {

using ::base::test::ErrorIs;


void VerifyErrorMessageResultForAllDeviceAttributesAPIs(
    blink::mojom::DeviceAPIService* service,
    const std::string& expected_error_message) {
  base::test::TestFuture<
      base::expected<blink::mojom::DeviceAttributeValuePtr, std::string>>
      future;

  service->GetDirectoryId(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(expected_error_message));

  service->GetHostname(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(expected_error_message));

  service->GetSerialNumber(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(expected_error_message));

  service->GetAnnotatedAssetId(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(expected_error_message));

  service->GetAnnotatedLocation(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(expected_error_message));
}


}  // namespace

class DeviceAPIServiceWebAppTest : public DeviceAPIServiceTest,
                                   public WebAppTest {
 public:
  DeviceAPIServiceWebAppTest()
      : WebAppTest(WebAppTest::WithTestUrlLoaderFactory()) {
    account_id_ = AccountId::FromUserEmail(kUserEmail);
  }

  void SetUp() override {
    WebAppTest::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    InstallTrustedApps();
    profile()->SetPermissionControllerDelegate(
        permissions::GetPermissionControllerDelegate(profile()));
  }

  void InstallTrustedApps() {
    DeviceAPIServiceTest::InstallTrustedApps(profile());
  }

  void UninstallAllApps() { web_app::test::UninstallAllWebApps(profile()); }

  webapps::AppId UserInstallWebApp() {
    auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL(kDefaultAppInstallUrl));

    return web_app::test::InstallWebApp(
        profile(), std::move(app_info),
        /*overwrite_existing_manifest_fields=*/false,
        webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  }


  void TryCreatingService(
      const GURL& url,
      std::unique_ptr<DeviceAttributeApi> device_attribute_api) {
    DeviceAPIServiceTest::TryCreatingService(
        url, std::move(device_attribute_api), web_contents());
  }

  void VerifyErrorMessageResultForAllDeviceAttributesAPIs(
      const std::string& expected_error_message) {
    ::VerifyErrorMessageResultForAllDeviceAttributesAPIs(
        remote()->get(), expected_error_message);
  }

  const AccountId& account_id() const { return account_id_; }

  web_app::WebAppProvider* provider() {
    return web_app::WebAppProvider::GetForTest(profile());
  }

  void TearDown() override {
    provider()->Shutdown();
    WebAppTest::TearDown();
  }

 private:
  AccountId account_id_;
};

TEST_F(DeviceAPIServiceWebAppTest, DoesNotConnectForTrustedApps) {
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWebAppTest, DoesNotConnectForIncognitoProfile) {
  profile_metrics::SetBrowserProfileType(
      profile(), profile_metrics::BrowserProfileType::kIncognito);
  TryCreatingService(GURL(kTrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWebAppTest, DoesNotConnectForUntrustedApps) {
  TryCreatingService(GURL(kUntrustedUrl),
                     std::make_unique<DeviceAttributeApiImpl>());
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}
