// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/model/isolation_data.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "components/webapps/isolated_web_apps/test_support/fake_iwa_runtime_data_provider.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

using ::testing::_;

constexpr std::array<uint8_t, 32> kPublicKeyBytes1 = {
    0x01, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};

constexpr std::array<uint8_t, 32> kPublicKeyBytes2 = {
    0x02, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};

}  // namespace

class IsolatedWebAppTrustCheckerTest : public WebAppTest {
 public:
  IsolatedWebAppTrustCheckerTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
  }

  void SetUp() override {
    data_provider_reset_.emplace(
        IwaRuntimeDataProvider::SetInstanceForTesting(&data_provider_));
    WebAppTest::SetUp();
  }

  PrefService& pref_service() { return *profile()->GetPrefs(); }

  FakeIwaRuntimeDataProvider& data_provider() { return data_provider_; }

  const web_package::Ed25519PublicKey kPublicKey1 =
      web_package::Ed25519PublicKey::Create(base::span(kPublicKeyBytes1));
  const web_package::Ed25519PublicKey kPublicKey2 =
      web_package::Ed25519PublicKey::Create(base::span(kPublicKeyBytes2));

  const web_package::SignedWebBundleId kWebBundleId1 =
      web_package::SignedWebBundleId::CreateForPublicKey(kPublicKey1);
  const web_package::SignedWebBundleId kWebBundleId2 =
      web_package::SignedWebBundleId::CreateForPublicKey(kPublicKey2);

  const GURL kStartUrl1 =
      GURL(std::string(webapps::kIsolatedAppScheme) +
           url::kStandardSchemeSeparator + kWebBundleId1.id());
  const GURL kStartUrl2 =
      GURL(std::string(webapps::kIsolatedAppScheme) +
           url::kStandardSchemeSeparator + kWebBundleId2.id());

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeIwaRuntimeDataProvider data_provider_;
  std::optional<base::AutoReset<IwaRuntimeDataProvider*>> data_provider_reset_;
};

TEST_F(IsolatedWebAppTrustCheckerTest, DevWebBundleId) {
  EXPECT_THAT(
      IsolatedWebAppTrustChecker::IsOperationAllowed(
          *profile(),
          web_package::SignedWebBundleId::CreateRandomForProxyMode(),
          /*dev_mode=*/false,
          IwaInstallOperation{
              .source = webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER}),
      base::test::ErrorIs(_));
}

TEST_F(IsolatedWebAppTrustCheckerTest, UntrustedByDefault) {
  EXPECT_THAT(
      IsolatedWebAppTrustChecker::IsOperationAllowed(
          *profile(), kWebBundleId1, /*dev_mode=*/false,
          IwaInstallOperation{
              .source = webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER}),
      base::test::ErrorIs(_));

  EXPECT_THAT(
      IsolatedWebAppTrustChecker::IsOperationAllowed(
          *profile(), kWebBundleId2, /*dev_mode=*/false,
          IwaInstallOperation{
              .source = webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER}),
      base::test::ErrorIs(_));
}

TEST_F(IsolatedWebAppTrustCheckerTest,
       DevModeDoesNotAutomaticallyTrustAllApps) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kIsolatedWebAppDevMode);

  EXPECT_THAT(
      IsolatedWebAppTrustChecker::IsOperationAllowed(
          *profile(), kWebBundleId1, /*dev_mode=*/false,
          IwaInstallOperation{
              .source = webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER}),
      base::test::ErrorIs(_));
}

TEST_F(IsolatedWebAppTrustCheckerTest, TrustedViaDevMode) {
  EXPECT_THAT(IsolatedWebAppTrustChecker::IsOperationAllowed(
                  *profile(), kWebBundleId1, /*dev_mode=*/true,
                  IwaInstallOperation{
                      .source = webapps::WebappInstallSource::IWA_DEV_UI}),
              base::test::ErrorIs(_));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kIsolatedWebAppDevMode);
  EXPECT_THAT(IsolatedWebAppTrustChecker::IsOperationAllowed(
                  *profile(), kWebBundleId1, /*dev_mode=*/true,
                  IwaInstallOperation{
                      .source = webapps::WebappInstallSource::IWA_DEV_UI}),
              base::test::HasValue());

  pref_service().SetInteger(
      prefs::kDevToolsAvailability,
      std::to_underlying(
          policy::DeveloperToolsAvailability::kDisallowed));

  EXPECT_THAT(IsolatedWebAppTrustChecker::IsOperationAllowed(
                  *profile(), kWebBundleId1, /*dev_mode=*/true,
                  IwaInstallOperation{
                      .source = webapps::WebappInstallSource::IWA_DEV_UI}),
              base::test::ErrorIs(_));
}

TEST_F(IsolatedWebAppTrustCheckerTest, TrustedWebBundleIDsForTesting) {
  SetTrustedWebBundleIdsForTesting({kWebBundleId1});

  EXPECT_THAT(
      IsolatedWebAppTrustChecker::IsOperationAllowed(
          *profile(), kWebBundleId1, /*dev_mode=*/false,
          IwaInstallOperation{
              .source = webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER}),
      base::test::HasValue());

  EXPECT_THAT(
      IsolatedWebAppTrustChecker::IsOperationAllowed(
          *profile(), kWebBundleId2, /*dev_mode=*/false,
          IwaInstallOperation{
              .source = webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER}),
      base::test::ErrorIs(_));
}

TEST_F(IsolatedWebAppTrustCheckerTest, ResourceLoadingForInstalledApp) {
  std::optional<webapps::ManifestId> manifest_id =
      webapps::ManifestId::Create(kStartUrl1);
  EXPECT_TRUE(manifest_id.has_value());
  auto iwa = std::make_unique<WebApp>(*manifest_id, kStartUrl1, kStartUrl1);
  iwa->SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle("dir_name", /*dev_mode=*/false),
          IwaVersion::Create("1.0.0").value())
          .Build());

  // Installed apps are trusted for resource loading by default.
  EXPECT_THAT(IsolatedWebAppTrustChecker::IsResourceLoadingAllowed(
                  *profile(), kWebBundleId1, *iwa),
              base::test::HasValue());
}

}  // namespace web_app
