// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/geolocation_permission_context.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/id_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/with_feature_override.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"
#include "components/permissions/resolvers/geolocation_permission_resolver.h"
#include "components/permissions/resolvers/permission_resolver.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "services/device/public/cpp/geolocation/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "url/origin.h"

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#include "components/permissions/contexts/geolocation_permission_context_system.h"
#include "services/device/public/cpp/test/fake_geolocation_system_permission_manager.h"
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

using content::MockRenderProcessHost;

namespace permissions {
namespace {

using blink::mojom::PermissionStatus;

class TestGeolocationPermissionContextDelegate
    : public GeolocationPermissionContext::Delegate {
 public:
  explicit TestGeolocationPermissionContextDelegate(
      content::BrowserContext* browser_context) {}

  bool DecidePermission(const PermissionRequestData& request_data,
                        BrowserPermissionCallback* callback,
                        GeolocationPermissionContext* context) override {
    return false;
  }

  void SetDSEOriginForTesting(const url::Origin& dse_origin) {
    dse_origin_ = dse_origin;
  }

 private:
  TestingPrefServiceSimple prefs_;
  std::optional<url::Origin> dse_origin_;
};
}  // namespace

// GeolocationPermissionContextTests ------------------------------------------

class GeolocationPermissionContextTestsBase
    : public content::RenderViewHostTestHarness,
      public permissions::Observer {
 public:
  GeolocationPermissionContextTestsBase();

 protected:
  // RenderViewHostTestHarness:
  void SetUp() override;
  void TearDown() override;
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override;

  PermissionRequestID RequestID(int request_id);
  PermissionRequestID RequestIDForTab(int tab, int request_id);

  void RequestGeolocationPermission(
      const PermissionRequestID& id,
      const GURL& requesting_frame,
      bool user_gesture,
      bool embedded_permission_element_initiated = false,
      blink::mojom::PermissionName permission_name =
          blink::mojom::PermissionName::GEOLOCATION);

  blink::mojom::PermissionStatus GetPermissionStatus(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      const GURL& requesting_origin);

  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission_descriptor,
      const GURL& requesting_origin);

  void PermissionResponse(const PermissionRequestID& id,
                          content::PermissionResult permission_result);
  void CheckPermissionMessageSent(int request_id, bool allowed);
  void CheckPermissionMessageSentForTab(int tab, int request_id, bool allowed);
  void CheckPermissionMessageSentInternal(MockRenderProcessHost* process,
                                          int request_id,
                                          bool allowed);
  void AddNewTab(const GURL& url);
  void CheckTabContentsState(const GURL& requesting_frame,
                             ContentSetting expected_content_setting);
  void SetupRequestManager(content::WebContents* web_contents);

  // permissions::Observer:
  void OnPermissionChanged(const ContentSettingsPattern& primary_pattern,
                           const ContentSettingsPattern& secondary_pattern,
                           ContentSettingsTypeSet content_type_set) override;

  void RequestManagerDocumentLoadCompleted();
  void RequestManagerDocumentLoadCompleted(content::WebContents* web_contents);
  void ExpectGeolocationPermissionSettingAsk(const GURL& frame_0,
                                             const GURL& frame_1);
  void SetGeolocationContentSetting(const GURL& frame_0,
                                    const GURL& frame_1,
                                    ContentSetting content_setting);
  bool HasActivePrompt();
  bool HasActivePrompt(content::WebContents* web_contents);
  void AcceptPrompt();
  void AcceptPrompt(content::WebContents* web_contents);
  void AcceptPromptThisTime();
  void DenyPrompt();
  void ClosePrompt();
  std::u16string GetPromptText();

  TestPermissionsClient client_;
  // owned by |BrowserContest::GetPermissionControllerDelegate()|
  raw_ptr<GeolocationPermissionContext, AcrossTasksDanglingUntriaged>
      geolocation_permission_context_ = nullptr;
  // owned by |geolocation_permission_context_|
  raw_ptr<TestGeolocationPermissionContextDelegate,
          AcrossTasksDanglingUntriaged>
      delegate_ = nullptr;
  std::vector<std::unique_ptr<content::WebContents>> extra_tabs_;
  std::vector<std::unique_ptr<MockPermissionPromptFactory>>
      mock_permission_prompt_factories_;

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  raw_ptr<device::FakeGeolocationSystemPermissionManager>
      fake_geolocation_system_permission_manager_;
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

  // A map between renderer child id and a pair represending the bridge id and
  // whether the requested permission was allowed.
  std::map<content::ChildProcessId,
           std::pair<PermissionRequestID::RequestLocalId, bool>>
      responses_;
  int num_permission_updates_ = 0;
  raw_ptr<ContentSettingsPattern> expected_primary_pattern_ = nullptr;
  raw_ptr<ContentSettingsPattern> expected_secondary_pattern_ = nullptr;
  std::vector<std::string> events_;
};

GeolocationPermissionContextTestsBase::GeolocationPermissionContextTestsBase() =
    default;

class GeolocationPermissionContextTests
    : public base::test::WithFeatureOverride,
      public GeolocationPermissionContextTestsBase {
 public:
  GeolocationPermissionContextTests()
      : base::test::WithFeatureOverride(
            content_settings::features::kApproximateGeolocationPermission) {}
};

class ApproximateOnlyGeolocationPermissionContextTests
    : public GeolocationPermissionContextTestsBase {
  base::test::ScopedFeatureList enable_approx_location_{
      content_settings::features::kApproximateGeolocationPermission};
};

PermissionRequestID GeolocationPermissionContextTestsBase::RequestID(
    int request_id) {
  return PermissionRequestID(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
      PermissionRequestID::RequestLocalId(request_id));
}

PermissionRequestID GeolocationPermissionContextTestsBase::RequestIDForTab(
    int tab,
    int request_id) {
  return PermissionRequestID(
      extra_tabs_[tab]->GetPrimaryMainFrame()->GetGlobalId(),
      PermissionRequestID::RequestLocalId(request_id));
}

void GeolocationPermissionContextTestsBase::RequestGeolocationPermission(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture,
    bool embedded_permission_element_initiated,
    blink::mojom::PermissionName permission_name) {
  auto request_data = std::make_unique<permissions::PermissionRequestData>(
      blink::mojom::PermissionDescriptor::New(permission_name,
                                              /*extension=*/nullptr),
      id, user_gesture, requesting_frame);

  if (embedded_permission_element_initiated) {
    request_data->embedded_permission_request_descriptor =
        blink::mojom::EmbeddedPermissionRequestDescriptor::New();
    if (permission_name ==
            blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE ||
        permission_name == blink::mojom::PermissionName::GEOLOCATION) {
      request_data->embedded_permission_request_descriptor->detail = blink::
          mojom::EmbeddedPermissionControlDescriptorExtension::NewGeolocation(
              blink::mojom::GeolocationEmbeddedPermissionRequestDescriptor::New(
                  /*autolocate=*/false));
    }
  }
  geolocation_permission_context_->RequestPermission(
      std::move(request_data),
      base::BindOnce(&GeolocationPermissionContextTestsBase::PermissionResponse,
                     base::Unretained(this), id));
  content::RunAllTasksUntilIdle();
}

blink::mojom::PermissionStatus
GeolocationPermissionContextTestsBase::GetPermissionStatus(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const GURL& requesting_origin) {
  return browser_context()
      ->GetPermissionController()
      ->GetPermissionResultForOriginWithoutContext(
          permission_descriptor, url::Origin::Create(requesting_origin))
      .status;
}

blink::mojom::PermissionStatus
GeolocationPermissionContextTestsBase::GetPermissionStatus(
    blink::PermissionType permission,
    const GURL& requesting_origin) {
  return GetPermissionStatus(
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(permission),
      requesting_origin);
}

void GeolocationPermissionContextTestsBase::PermissionResponse(
    const PermissionRequestID& id,
    content::PermissionResult permission_result) {
  LOG(ERROR) << "GeolocationPermissionContextTestsBase::PermissionResponse "
             << id.ToString() << " " << permission_result.status;
  responses_[id.global_render_frame_host_id().child_id] =
      std::make_pair(id.request_local_id_for_testing(),
                     permission_result.status == PermissionStatus::GRANTED);
  events_.push_back("PermissionResponse");
}

void GeolocationPermissionContextTestsBase::OnPermissionChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  EXPECT_TRUE(primary_pattern.IsValid());
  EXPECT_TRUE(secondary_pattern.IsValid());
  EXPECT_EQ(*expected_primary_pattern_, primary_pattern);
  EXPECT_EQ(*expected_secondary_pattern_, secondary_pattern);
  EXPECT_EQ(content_type_set.GetType(),
            content_settings::GeolocationContentSettingsType());
  num_permission_updates_++;
  events_.push_back("OnPermissionChanged");
}

void GeolocationPermissionContextTestsBase::CheckPermissionMessageSent(
    int request_id,
    bool allowed) {
  CheckPermissionMessageSentInternal(process(), request_id, allowed);
}

void GeolocationPermissionContextTestsBase::CheckPermissionMessageSentForTab(
    int tab,
    int request_id,
    bool allowed) {
  CheckPermissionMessageSentInternal(
      static_cast<MockRenderProcessHost*>(
          extra_tabs_[tab]->GetPrimaryMainFrame()->GetProcess()),
      request_id, allowed);
}

void GeolocationPermissionContextTestsBase::CheckPermissionMessageSentInternal(
    MockRenderProcessHost* process,
    int request_id,
    bool allowed) {
  ASSERT_EQ(responses_.count(process->GetID()), 1U);
  EXPECT_EQ(PermissionRequestID::RequestLocalId(request_id),
            responses_[process->GetID()].first);
  EXPECT_EQ(allowed, responses_[process->GetID()].second);
  responses_.erase(process->GetID());
}

void GeolocationPermissionContextTestsBase::AddNewTab(const GURL& url) {
  std::unique_ptr<content::WebContents> new_tab = CreateTestWebContents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(new_tab.get(),
                                                             url);
  SetupRequestManager(new_tab.get());

  extra_tabs_.push_back(std::move(new_tab));
}

void GeolocationPermissionContextTestsBase::CheckTabContentsState(
    const GURL& requesting_frame,
    ContentSetting expected_content_setting) {
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(expected_content_setting == CONTENT_SETTING_BLOCK
                  ? content_settings->IsContentBlocked(
                        content_settings::GeolocationContentSettingsType())
                  : content_settings->IsContentAllowed(
                        content_settings::GeolocationContentSettingsType()));
}

std::unique_ptr<content::BrowserContext>
GeolocationPermissionContextTestsBase::CreateBrowserContext() {
  std::unique_ptr<content::TestBrowserContext> test_browser_contest =
      std::make_unique<content::TestBrowserContext>();
  test_browser_contest->SetPermissionControllerDelegate(
      permissions::GetPermissionControllerDelegate(test_browser_contest.get()));
  return test_browser_contest;
}

void GeolocationPermissionContextTestsBase::SetUp() {
  RenderViewHostTestHarness::SetUp();

  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<
          content_settings::TestPageSpecificContentSettingsDelegate>(
          /*prefs=*/nullptr,
          PermissionsClient::Get()->GetSettingsMap(browser_context())));

  auto delegate = std::make_unique<TestGeolocationPermissionContextDelegate>(
      browser_context());
  delegate_ = delegate.get();

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  auto fake_geolocation_system_permission_manager =
      std::make_unique<device::FakeGeolocationSystemPermissionManager>();
  fake_geolocation_system_permission_manager_ =
      fake_geolocation_system_permission_manager.get();
  fake_geolocation_system_permission_manager->SetSystemPermission(
      device::LocationSystemPermissionStatus::kAllowed);
  device::FakeGeolocationSystemPermissionManager::SetInstance(
      std::move(fake_geolocation_system_permission_manager));
  auto context = std::make_unique<GeolocationPermissionContextSystem>(
      browser_context(), std::move(delegate));
#else
  auto context = std::make_unique<GeolocationPermissionContext>(
      browser_context(), std::move(delegate));
#endif
  SetupRequestManager(web_contents());

  geolocation_permission_context_ = context.get();

  PermissionManager* permission_manager = static_cast<PermissionManager*>(
      browser_context()->GetPermissionControllerDelegate());
  permission_manager->PermissionContextsForTesting()
      [content_settings::GeolocationContentSettingsType()] = std::move(context);
}

void GeolocationPermissionContextTestsBase::TearDown() {
  mock_permission_prompt_factories_.clear();
  extra_tabs_.clear();
  DeleteContents();
  RenderViewHostTestHarness::TearDown();
}

void GeolocationPermissionContextTestsBase::SetupRequestManager(
    content::WebContents* web_contents) {
  // Create PermissionRequestManager.
  PermissionRequestManager::CreateForWebContents(web_contents);
  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);

  // Create a MockPermissionPromptFactory for the PermissionRequestManager.
  mock_permission_prompt_factories_.push_back(
      std::make_unique<MockPermissionPromptFactory>(
          permission_request_manager));
}

void GeolocationPermissionContextTestsBase::
    RequestManagerDocumentLoadCompleted() {
  GeolocationPermissionContextTestsBase::RequestManagerDocumentLoadCompleted(
      web_contents());
}

void GeolocationPermissionContextTestsBase::RequestManagerDocumentLoadCompleted(
    content::WebContents* web_contents) {
  PermissionRequestManager::FromWebContents(web_contents)
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
}

void GeolocationPermissionContextTestsBase::
    ExpectGeolocationPermissionSettingAsk(const GURL& frame_0,
                                          const GURL& frame_1) {
  PermissionSetting permission_setting =
      PermissionsClient::Get()
          ->GetSettingsMap(browser_context())
          ->GetPermissionSetting(
              frame_0, frame_1,
              content_settings::GeolocationContentSettingsType());
  if (auto* content_setting =
          std::get_if<ContentSetting>(&permission_setting)) {
    EXPECT_EQ(CONTENT_SETTING_ASK, *content_setting);
  } else if (auto* geolocation_setting =
                 std::get_if<GeolocationSetting>(&permission_setting)) {
    EXPECT_EQ(
        (GeolocationSetting{PermissionOption::kAsk, PermissionOption::kAsk}),
        *geolocation_setting);
  } else {
    NOTREACHED();
  }
}

void GeolocationPermissionContextTestsBase::SetGeolocationContentSetting(
    const GURL& frame_0,
    const GURL& frame_1,
    ContentSetting content_setting) {
  PermissionSetting permission_setting =
      content_settings::PermissionSettingsRegistry::GetInstance()
          ->Get(content_settings::GeolocationContentSettingsType())
          ->delegate()
          .ToPermissionSetting(content_setting);
  return PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->SetPermissionSettingDefaultScope(
          frame_0, frame_1, content_settings::GeolocationContentSettingsType(),
          permission_setting);
}

bool GeolocationPermissionContextTestsBase::HasActivePrompt() {
  return HasActivePrompt(web_contents());
}

bool GeolocationPermissionContextTestsBase::HasActivePrompt(
    content::WebContents* web_contents) {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents);
  return manager->IsRequestInProgress();
}

void GeolocationPermissionContextTestsBase::AcceptPrompt() {
  return AcceptPrompt(web_contents());
}

void GeolocationPermissionContextTestsBase::AcceptPrompt(
    content::WebContents* web_contents) {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents);
  manager->Accept(content_settings::GeolocationContentSettingsType() ==
                          ContentSettingsType::GEOLOCATION_WITH_OPTIONS
                      ? PromptOptions(GeolocationPromptOptions{
                            .selected_accuracy = GeolocationAccuracy::kPrecise})
                      : std::monostate());
  base::RunLoop().RunUntilIdle();
}

void GeolocationPermissionContextTestsBase::AcceptPromptThisTime() {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  manager->AcceptThisTime(
      content_settings::GeolocationContentSettingsType() ==
              ContentSettingsType::GEOLOCATION_WITH_OPTIONS
          ? PromptOptions(GeolocationPromptOptions{
                .selected_accuracy = GeolocationAccuracy::kPrecise})
          : std::monostate());
  base::RunLoop().RunUntilIdle();
}

void GeolocationPermissionContextTestsBase::DenyPrompt() {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  manager->Deny(/*prompt_options=*/std::monostate());
  base::RunLoop().RunUntilIdle();
}

void GeolocationPermissionContextTestsBase::ClosePrompt() {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  manager->Dismiss(/*prompt_options=*/std::monostate());
  base::RunLoop().RunUntilIdle();
}

std::u16string GeolocationPermissionContextTestsBase::GetPromptText() {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  auto& request = manager->Requests().front();
  return base::ASCIIToUTF16(request->requesting_origin().spec()) +
         request->GetMessageTextFragment();
}

// Tests ----------------------------------------------------------------------

TEST_P(GeolocationPermissionContextTests, SinglePermissionPrompt) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
}

TEST_P(GeolocationPermissionContextTests, ApproximatePermissionPropagated) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());

  // Request approximate location.
  RequestGeolocationPermission(
      RequestID(0), requesting_frame, /*user_gesture=*/true,
      /*embedded_permission_element_initiated=*/false,
      blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE);

  ASSERT_TRUE(HasActivePrompt());

  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  manager->Accept(
      content_settings::GeolocationContentSettingsType() ==
              ContentSettingsType::GEOLOCATION_WITH_OPTIONS
          ? PromptOptions(GeolocationPromptOptions{
                .selected_accuracy = GeolocationAccuracy::kApproximate})
          : std::monostate());
  EXPECT_FALSE(HasActivePrompt());

  // On Android, if approximate location was requested, it should be granted
  // for GEOLOCATION_APPROXIMATE.
  CheckPermissionMessageSent(0, true);
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION_APPROXIMATE,
                                requesting_frame));
}

TEST_F(ApproximateOnlyGeolocationPermissionContextTests,
       ApproximateGeolocationPromptText) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(
      RequestID(0), requesting_frame, true,
      /*embedded_permission_element_initiated=*/true,
      blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE);
  ASSERT_TRUE(HasActivePrompt());
}

TEST_P(GeolocationPermissionContextTests,
       SinglePermissionPromptFailsOnInsecureOrigin) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  ASSERT_FALSE(HasActivePrompt());
}

TEST_P(GeolocationPermissionContextTests, HashIsIgnored) {
  GURL url_a("https://www.example.com/geolocation#a");
  GURL url_b("https://www.example.com/geolocation#b");

  // Navigate to the first url.
  NavigateAndCommit(url_a);
  RequestManagerDocumentLoadCompleted();

  // Check permission is requested.
  ASSERT_FALSE(HasActivePrompt());
  const bool user_gesture = true;
  RequestGeolocationPermission(RequestID(0), url_a, user_gesture);
  ASSERT_TRUE(HasActivePrompt());

  // Change the hash, we'll still be on the same page.
  NavigateAndCommit(url_b);
  RequestManagerDocumentLoadCompleted();

  // Accept.
  AcceptPrompt();
  CheckTabContentsState(url_a, CONTENT_SETTING_ALLOW);
  CheckTabContentsState(url_b, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
}

TEST_P(GeolocationPermissionContextTests, DISABLED_PermissionForFileScheme) {
  // TODO(felt): The bubble is rejecting file:// permission requests.
  // Fix and enable this test. crbug.com/444047
  GURL requesting_frame("file://example/geolocation.html");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  // Check permission is requested.
  ASSERT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  EXPECT_TRUE(HasActivePrompt());

  // Accept the frame.
  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  // Make sure the setting is not stored.
  ExpectGeolocationPermissionSettingAsk(requesting_frame, requesting_frame);
}

TEST_P(GeolocationPermissionContextTests, CancelGeolocationPermissionRequest) {
  GURL frame_0("https://www.example.com/geolocation");
  ExpectGeolocationPermissionSettingAsk(frame_0, frame_0);

  NavigateAndCommit(frame_0);
  RequestManagerDocumentLoadCompleted();

  ASSERT_FALSE(HasActivePrompt());

  RequestGeolocationPermission(RequestID(0), frame_0, true);

  ASSERT_TRUE(HasActivePrompt());
  std::u16string text_0 = GetPromptText();
  ASSERT_FALSE(text_0.empty());

  // Simulate the frame going away; the request should be removed.
  ClosePrompt();

  // Ensure permission isn't persisted.
  ExpectGeolocationPermissionSettingAsk(frame_0, frame_0);
}

TEST_P(GeolocationPermissionContextTests, InvalidURL) {
  // Navigate to the first url.
  GURL invalid_embedder("about:blank");
  GURL requesting_frame;
  NavigateAndCommit(invalid_embedder);
  RequestManagerDocumentLoadCompleted();

  // Nothing should be displayed.
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  EXPECT_FALSE(HasActivePrompt());
  CheckPermissionMessageSent(0, false);
}

TEST_P(GeolocationPermissionContextTests, SameOriginMultipleTabs) {
  GURL url_a("https://www.example.com/geolocation");
  GURL url_b("https://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);  // Tab A0
  AddNewTab(url_b);          // Tab B (extra_tabs_[0])
  AddNewTab(url_a);          // Tab A1 (extra_tabs_[1])
  RequestManagerDocumentLoadCompleted();
  RequestManagerDocumentLoadCompleted(extra_tabs_[0].get());
  RequestManagerDocumentLoadCompleted(extra_tabs_[1].get());

  // Request permission in all three tabs.
  RequestGeolocationPermission(RequestID(0), url_a, true);
  RequestGeolocationPermission(RequestIDForTab(0, 0), url_b, true);
  RequestGeolocationPermission(RequestIDForTab(1, 0), url_a, true);
  ASSERT_TRUE(HasActivePrompt());  // For A0.
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[0].get()));
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[1].get()));

  // Accept the permission in tab A0.
  AcceptPrompt();
  CheckPermissionMessageSent(0, true);
  // Because they're the same origin, this should cause tab A1's prompt
  // to disappear, but it doesn't: crbug.com/443013.
  // TODO(felt): Update this test when the bubble's behavior is changed.
  // Either way, tab B should still have a pending permission request.
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[0].get()));
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[1].get()));
}

TEST_P(GeolocationPermissionContextTests, TabDestroyed) {
  GURL requesting_frame("https://www.example.com/geolocation");
  ExpectGeolocationPermissionSettingAsk(requesting_frame, requesting_frame);

  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  // Request permission for two frames.
  RequestGeolocationPermission(RequestID(0), requesting_frame, false);

  ASSERT_TRUE(HasActivePrompt());
  ExpectGeolocationPermissionSettingAsk(requesting_frame, requesting_frame);
}

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
TEST_P(GeolocationPermissionContextTests,
       AllSystemAndSitePermissionCombinations) {
  GURL requesting_frame("https://www.example.com/geolocation");

  const struct {
    const LocationSystemPermissionStatus system_permission;
    const ContentSetting site_permission;
    const PermissionStatus expected_effective_site_permission;
  } kTestCases[] = {
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kDenied),
       ContentSetting(CONTENT_SETTING_ASK), PermissionStatus::ASK},
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kDenied),
       ContentSetting(CONTENT_SETTING_BLOCK), PermissionStatus::DENIED},
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kDenied),
       ContentSetting(CONTENT_SETTING_ALLOW), PermissionStatus::DENIED},
      {LocationSystemPermissionStatus(
           LocationSystemPermissionStatus::kNotDetermined),
       ContentSetting(CONTENT_SETTING_ASK), PermissionStatus::ASK},
      {LocationSystemPermissionStatus(
           LocationSystemPermissionStatus::kNotDetermined),
       ContentSetting(CONTENT_SETTING_BLOCK), PermissionStatus::DENIED},
      {LocationSystemPermissionStatus(
           LocationSystemPermissionStatus::kNotDetermined),
       ContentSetting(CONTENT_SETTING_ALLOW), PermissionStatus::ASK},
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kAllowed),
       ContentSetting(CONTENT_SETTING_ASK), PermissionStatus::ASK},
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kAllowed),
       ContentSetting(CONTENT_SETTING_BLOCK), PermissionStatus::DENIED},
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kAllowed),
       ContentSetting(CONTENT_SETTING_ALLOW), PermissionStatus::GRANTED},
  };

  for (auto test_case : kTestCases) {
    SetGeolocationContentSetting(requesting_frame, requesting_frame,
                                 test_case.site_permission);
    fake_geolocation_system_permission_manager_->SetSystemPermission(
        test_case.system_permission);
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(test_case.expected_effective_site_permission,
              GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                  requesting_frame));
  }
}

TEST_P(GeolocationPermissionContextTests, SystemPermissionUpdates) {
  GURL requesting_frame("https://www.example.com/geolocation");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLNoWildcard(requesting_frame);
  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();

  geolocation_permission_context_->AddObserver(this);
  expected_primary_pattern_ = &primary_pattern;
  expected_secondary_pattern_ = &secondary_pattern;
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);
  ASSERT_EQ(1, num_permission_updates_);
  primary_pattern = ContentSettingsPattern::Wildcard();
  fake_geolocation_system_permission_manager_->SetSystemPermission(
      LocationSystemPermissionStatus::kDenied);
  fake_geolocation_system_permission_manager_->SetSystemPermission(
      LocationSystemPermissionStatus::kAllowed);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3, num_permission_updates_);
  geolocation_permission_context_->RemoveObserver(this);
}
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

TEST_P(GeolocationPermissionContextTests, DecisionEventOrder) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  // Clear any previous event order.
  events_.clear();
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLNoWildcard(requesting_frame);
  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();
  expected_primary_pattern_ = &primary_pattern;
  expected_secondary_pattern_ = &secondary_pattern;
  geolocation_permission_context_->AddObserver(this);
  RequestGeolocationPermission(RequestID(0), requesting_frame, true,
                               /*embedded_permission_element_initiated=*/true);
  AcceptPrompt();
  content::RunAllTasksUntilIdle();
  CheckPermissionMessageSent(0, true);
  geolocation_permission_context_->RemoveObserver(this);

  // Verify the order of events. OnPermissionChanged should be called before
  // PermissionResponse.
  ASSERT_EQ(2U, events_.size());
  EXPECT_EQ("OnPermissionChanged", events_[0]);
  EXPECT_EQ("PermissionResponse", events_[1]);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(GeolocationPermissionContextTests);

}  // namespace permissions
