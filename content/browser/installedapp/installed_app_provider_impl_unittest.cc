// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/installedapp/installed_app_provider_impl.h"

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/installedapp/installed_app_provider_impl.h"
#include "content/browser/installedapp/test/installed_app_provider_impl_test_utils.h"
#include "content/common/features.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

namespace content {

namespace {

using testing::Contains;
using testing::IsEmpty;
using testing::Not;

const char kInstalledWinAppId[] = "a";
const char kInstalledWebAppId[] = "http://foo.com/";

const std::vector<std::string> kInstalledWinAppIds = {
    kInstalledWinAppId, "B", "C", "D", "E", "F", "G"};
const std::vector<std::string> kInstalledWebAppIds = {
    kInstalledWebAppId, "http://foo.com/test", "http://foo2.com",
    "http://bar.com"};
}  // namespace

class RelatedAppsTestWebContentsDelegate : public WebContentsDelegate {
 public:
  MOCK_METHOD(std::vector<blink::mojom::RelatedApplicationPtr>,
              GetSavedRelatedApplications,
              (content::WebContents*),
              (override));
};

class InstalledAppProviderImplTest : public RenderViewHostImplTestHarness {
 public:
  explicit InstalledAppProviderImplTest()
      : content_browser_client_(kInstalledWebAppIds) {
    feature_list_.InitWithFeatures(
        {features::kInstalledAppProvider,
         features::kFilterInstalledAppsWebAppMatching},
        {});
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    old_content_browser_client_ =
        SetBrowserClientForTesting(&content_browser_client_);

    // Owned ptr, we must be careful to destroy this on TearDown().
    provider_ = InstalledAppProviderImpl::CreateForTesting(
        *(contents()->GetPrimaryMainFrame()),
        remote_.BindNewPipeAndPassReceiver());
    web_contents()->SetDelegate(&web_contents_delegate_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_content_browser_client_);
    // Owned ptr, we must be careful to destroy this on TearDown().
    provider_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  mojo::Remote<blink::mojom::InstalledAppProvider>& remote() { return remote_; }

  RelatedAppsTestWebContentsDelegate* web_contents_delegate() {
    return &web_contents_delegate_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  mojo::Remote<blink::mojom::InstalledAppProvider> remote_;
  FakeContentBrowserClientForQueryInstalledWebApps content_browser_client_;
  raw_ptr<InstalledAppProviderImpl> provider_;
  raw_ptr<content::ContentBrowserClient> old_content_browser_client_ = nullptr;
  RelatedAppsTestWebContentsDelegate web_contents_delegate_;
};

MATCHER_P(RelatedAppById, app_id, "") {
  if (app_id != arg->id) {
    *result_listener << arg->id.value() << " doesn't match expected " << app_id;
  }
  return arg->id == app_id;
}

TEST_F(InstalledAppProviderImplTest, GetRelatedApps) {
  std::vector<blink::mojom::RelatedApplicationPtr> related_applications;
  const std::string unknown_web_app_id = "http://unknownid.com";

  // Test that related application matching on windows does not rely on
  // capitalization.
  related_applications.push_back(CreateRelatedApplicationFromPlatformAndId(
      "windows", base::ToUpperASCII(kInstalledWinAppId)));
  related_applications.push_back(
      CreateRelatedApplicationFromPlatformAndId("webapp", kInstalledWebAppId));
  related_applications.push_back(
      CreateRelatedApplicationFromPlatformAndId("webapp", unknown_web_app_id));

  base::test::TestFuture<std::vector<blink::mojom::RelatedApplicationPtr>>
      future;

  remote()->FilterInstalledApps(
      std::move(related_applications), GURL("http://foo.com/manifest.json"),
      /*add_saved_related_applications=*/false, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const std::vector<blink::mojom::RelatedApplicationPtr>& result = future.Get();

  std::size_t expected_number_of_matches = 0u;
#if !BUILDFLAG(IS_ANDROID)
  expected_number_of_matches += 1u;
#endif  // !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(result.size(), expected_number_of_matches);

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(result, Contains(RelatedAppById(kInstalledWebAppId)));
#endif  // !BUILDFLAG(IS_ANDROID)

  // Should not be in list.
  EXPECT_THAT(result, Not(Contains(RelatedAppById(unknown_web_app_id))));
}

TEST_F(InstalledAppProviderImplTest,
       ShouldReturnNothingWithEmptyRelatedApplications) {
  base::test::TestFuture<std::vector<blink::mojom::RelatedApplicationPtr>>
      future;

  // Empty related apps list
  remote()->FilterInstalledApps(
      std::vector<blink::mojom::RelatedApplicationPtr>(),
      GURL("http://foo.com/manifest.json"),
      /*add_saved_related_applications=*/false, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(InstalledAppProviderImplTest,
       ShouldNotReturnWebAppIfManifestIdIsInvalid) {
  const std::string invalid_web_app_id = "http:invalid-url";
  std::vector<blink::mojom::RelatedApplicationPtr> related_applications;
  related_applications.push_back(
      CreateRelatedApplicationFromPlatformAndId("webapp", invalid_web_app_id));

  base::test::TestFuture<std::vector<blink::mojom::RelatedApplicationPtr>>
      future;
  remote()->FilterInstalledApps(
      std::move(related_applications), GURL("http://foo.com/manifest.json"),
      /*add_saved_related_applications=*/false, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(), IsEmpty());
}

}  // namespace content
