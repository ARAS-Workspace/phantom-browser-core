// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/glic/host/context/glic_share_image_handler.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/launchservices_utils_mac.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;
using testing::AllOf;
using testing::AnyOfArray;
using testing::Contains;
using testing::Ge;
using testing::IsSupersetOf;
using testing::Le;
using testing::Not;

namespace {

class ContextMenuUiTest : public InteractiveBrowserTest {
 public:
  ContextMenuUiTest() = default;

  ContextMenuUiTest(const ContextMenuUiTest&) = delete;
  ContextMenuUiTest& operator=(const ContextMenuUiTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// This is a regression test for https://crbug.com/40200861.  It tests using
// "Open link in new tab" context menu item in a subframe, to follow a link
// that should stay in the same SiteInstance (e.g. "about:blank", or "data:"
// URL).  This test is somewhat similar to ChromeNavigationBrowserTest's
// ContextMenuNavigationToInvalidUrl testcase, but 1) uses a subframe, and 2)
// more accurately simulates what the product code does.
//
// The test is compiled out on Mac, because RenderViewContextMenuMacCocoa::Show
// requires running a nested message loop - this would undesirably yield control
// over the next steps to the OS.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ContextMenuUiTest,
                       ContextMenuNavigationToAboutBlankUrlInSubframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with a cross-site subframe.
  GURL start_url = embedded_test_server()->GetURL(
      "start.com", "/frame_tree/page_with_two_frames_remote_and_local.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe = content::ChildFrameAt(main_frame, 0);
  ASSERT_NE(main_frame->GetLastCommittedOrigin(),
            subframe->GetLastCommittedOrigin());
  ASSERT_EQ("bar.com", subframe->GetLastCommittedOrigin().host());

  // Prepare ContextMenuParams that correspond to a link to an about:blank URL
  // in the cross-site subframe.  This preparation to some extent
  // duplicates/replicates the code in RenderFrameHostImpl::ShowContextMenu.
  //
  // Note that the repro steps in https://crbug.com/40200861 resulted in a
  // navigation to about:blank#blocked because of how a navigation to
  // javascript: URL gets rewritten by RenderProcessHost::FilterURL calls.
  // Directly navigating to an about:blank URL is just as good for replicating a
  // discrepancy between `source_site_instance` and `initiator_origin` (without
  // relying on implementation details of FilterURL).
  GURL link_url("about:blank#blah");
  content::ContextMenuParams params;
  params.link_url = link_url;
  params.is_editable = false;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  params.page_url = main_frame->GetLastCommittedURL();
  params.frame_url = subframe->GetLastCommittedURL();
  content::RenderProcessHost* process = subframe->GetProcess();
  process->FilterURL(true, &params.link_url);
  process->FilterURL(true, &params.src_url);

  // Simulate opening a context menu.
  //
  // Note that we can't use TestRenderViewContextMenu (like some other tests),
  // because this wouldn't exercise the product code responsible for the
  // https://crbug.com/40200861 bug (it wouldn't go through
  // ChromeWebContentsViewDelegateViews::ShowContextMenu).
  std::unique_ptr<content::WebContentsViewDelegate> view_delegate =
      CreateWebContentsViewDelegate(web_contents);
  view_delegate->ShowContextMenu(*subframe, params);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return view_delegate->IsContextMenuShowingForTesting(); }));

  // Simulate using the context menu to "Open link in new tab".
  content::WebContents* new_web_contents = nullptr;
  {
    ui_test_utils::TabAddedWaiter tab_add(browser());
    view_delegate->ExecuteCommandForTesting(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                                            0);
    tab_add.Wait();
    int index_of_new_tab = browser()->tab_strip_model()->count() - 1;
    new_web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(index_of_new_tab);
  }

  // Verify that the load succeeded and was associated with the right
  // SiteInstance.
  EXPECT_TRUE(WaitForLoadStop(new_web_contents));
  EXPECT_EQ(link_url, new_web_contents->GetLastCommittedURL());
  EXPECT_EQ(new_web_contents->GetPrimaryMainFrame()->GetSiteInstance(),
            subframe->GetSiteInstance());
}
#endif  // !BUILDFLAG(IS_MAC)

// TODO(crbug.com/375048798): Once Kombucha framework supports querying elements
// inside iframe and fenced frame, convert the context menu tests to use
// Kombucha framework.
class ContextMenuFencedFrameTest : public ContextMenuUiTest {
 public:
  ContextMenuFencedFrameTest() = default;
  ~ContextMenuFencedFrameTest() override = default;
  ContextMenuFencedFrameTest(const ContextMenuFencedFrameTest&) = delete;
  ContextMenuFencedFrameTest& operator=(const ContextMenuFencedFrameTest&) =
      delete;

  // Defines the skeleton of set up method.
  void SetUpOnMainThread() override {
    ContextMenuUiTest::SetUpOnMainThread();

    embedded_https_test_server().AddDefaultHandlers(GetChromeTestDataDir());
    // Add content/test/data for cross_site_iframe_factory.html.
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "content/test/data");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);

    override_registration_ =
        web_app::OsIntegrationTestOverrideImpl::OverrideForTesting();
  }

  // Create a fenced frame which is navigated to `url`.
  content::RenderFrameHost* CreateFencedFrame(const GURL& url) {
    // Navigate fenced frame to a page with an anchor element.
    GURL main_url(embedded_https_test_server().GetURL(
        "a.test",
        base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                           url.spec().c_str())));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

    // Get fenced frame render frame host.
    std::vector<content::RenderFrameHost*> child_frames =
        fenced_frame_test_helper().GetChildFencedFrameHosts(
            primary_main_frame_host());
    EXPECT_EQ(child_frames.size(), 1u);
    content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
    EXPECT_EQ(fenced_frame_rfh->GetLastCommittedURL(), url);

    return fenced_frame_rfh;
  }

  // Create a fenced frame with a nested iframe. The nested iframe is navigated
  // to `url`.
  content::RenderFrameHost* CreateFencedFrameWithNestedIframe(const GURL& url) {
    // The page has a fenced frame with a nested iframe inside.
    GURL main_url(embedded_https_test_server().GetURL(
        "a.test",
        base::StringPrintf(
            "/cross_site_iframe_factory.html?a.test(a.test{fenced}(%s))",
            url.spec().c_str())));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

    // Get fenced frame render frame host.
    std::vector<content::RenderFrameHost*> child_frames =
        fenced_frame_test_helper().GetChildFencedFrameHosts(
            primary_main_frame_host());
    EXPECT_EQ(child_frames.size(), 1u);
    content::RenderFrameHost* fenced_frame_rfh = child_frames[0];

    return fenced_frame_rfh;
  }

  content::RenderFrameHost* primary_main_frame_host() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  void InstallTestWebApp(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url;
    web_app_info->title = u"Test app";
    web_app_info->description = u"Test description";
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;

    web_app::test::InstallWebApp(browser()->GetProfile(),
                                 std::move(web_app_info));
  }

  void CleanupWebApps() {
    web_app::test::UninstallAllWebApps(browser()->GetProfile());
    override_registration_.reset();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  // OS integration is needed to be able to launch web applications. This
  // override ensures OS integration doesn't leave any traces.
  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

// Check which commands are present after opening the context menu for a
// fencedframe.
// TODO(crbug.com/40273673): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MenuContentsVerification_Fencedframe \
  DISABLED_MenuContentsVerification_Fencedframe
#else
#define MAYBE_MenuContentsVerification_Fencedframe \
  MenuContentsVerification_Fencedframe
#endif
IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       MAYBE_MenuContentsVerification_Fencedframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Load a fenced frame.
  GURL fenced_frame_url(
      embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  auto* fenced_frame_rfh = fenced_frame_test_helper().CreateFencedFrame(
      primary_main_frame_host(), fenced_frame_url);

  // To avoid a flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Open a context menu.
  ContextMenuWaiter menu_observer;
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  // These coordinates are relative to the fenced frame's widget since we're
  // forwarding this event directly to the widget.
  mouse_event.SetPositionInWidget(50, 50);

  auto* fenced_frame_widget = fenced_frame_rfh->GetRenderWidgetHost();
  fenced_frame_widget->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  fenced_frame_widget->ForwardMouseEvent(mouse_event);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              IsSupersetOf({IDC_BACK, IDC_FORWARD, IDC_RELOAD, IDC_VIEW_SOURCE,
                            IDC_SAVE_PAGE, IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE,
                            IDC_CONTENT_CONTEXT_RELOADFRAME,
                            IDC_CONTENT_CONTEXT_INSPECTELEMENT}));
}

class GlicInteractiveContextMenuTestBase
    : public glic::test::InteractiveGlicTest {
 public:
  GlicInteractiveContextMenuTestBase() = default;
  ~GlicInteractiveContextMenuTestBase() override = default;

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->GetProfile());
    signin::MakePrimaryAccountAvailable(identity_manager, "foo@google.com",
                                        signin::ConsentLevel::kSignin);
    signin::SetRefreshTokenForPrimaryAccount(identity_manager);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);
  }

  auto PollForAndCompleteOnboarding() {
    return Steps(InAnyContext(PollUntil(
                     [this]() {
                       if (auto* instance = GetGlicInstanceImpl()) {
                         return instance->host().IsWebClientConnected();
                       }
                       return false;
                     },
                     "polling until the client is ready")),
                 Do([this]() {
                   ::glic::SetFRECompletion(browser()->GetProfile(),
                                            glic::prefs::FreStatus::kCompleted);
                 }));
  }

  auto PollForAndInstrumentGlic() {
    return Steps(UninstrumentWebContents(glic::kGlicContentsElementId, false),
                 UninstrumentWebContents(glic::kGlicHostElementId, false),
                 InAnyContext(Steps(
                     InstrumentNonTabWebView(glic::kGlicHostElementId,
                                             kGlicViewElementId),
                     InstrumentInnerWebContents(glic::kGlicContentsElementId,
                                                glic::kGlicHostElementId, 0),
                     WaitForWebContentsReady(glic::kGlicContentsElementId),
                     // TODO(b:448604727): State observation is currently
                     // unsupported with multi- instance, so we will poll.
                     PollUntil(
                         [this]() {
                           if (auto* instance = GetGlicInstanceImpl()) {
                             return instance->host().IsWebClientConnected();
                           }
                           return false;
                         },
                         "polling until web client is ready"))));
  }

  auto CheckHistograms() {
    return Do([this]() {
      histogram_tester_.ExpectUniqueSample(
          "Glic.TabContext.ShareImageResult",
          static_cast<int>(glic::ShareImageResult::kSentImageToClient), 1);
      EXPECT_THAT(
          histogram_tester_.GetAllSamples("Glic.TabContext.ShareImageDuration"),
          testing::SizeIs(1));
    });
  }

  auto CacheCurrentInstance() {
    return Do([this]() {
      if (glic::GlicInstance* instance =
              glic_service()->GetInstanceForActiveTab(browser())) {
        cached_instance_id_ = instance->id();
      }
    });
  }

  auto CheckCachedInstance() {
    return Do([this]() {
      glic::GlicInstance* instance =
          glic_service()->GetInstanceForActiveTab(browser());
      EXPECT_TRUE(instance->id().IsValid());
      EXPECT_TRUE(cached_instance_id_.IsValid());
      EXPECT_NE(instance->id(), cached_instance_id_);
      cached_instance_id_ = glic::InstanceId::CreateNullId();
    });
  }

  auto PollForNewGlicInstance() {
    auto steps = PollUntil(
        [this]() {
          auto* instance = glic_service()->GetInstanceForActiveTab(browser());
          return cached_instance_id_.IsValid() && instance &&
                 cached_instance_id_ != instance->id();
        },
        "polling we have a new glic instance");
    for (auto& step : steps) {
      step.SetMustRemainVisible(false);
    }
    return steps;
  }

  auto WaitForAdditionalContext() {
    auto steps = WaitForJsResult(
        glic::kGlicContentsElementId,
        "() => { "
        "  let c = document.querySelector('#additionalContextResult');"
        "  return !!c && c.children.length >= 4 && "
        "      c.innerText.includes('MIME Type: image/png');"
        "}");
    for (auto& step : steps) {
      step.SetMustRemainVisible(false);
    }
    return steps;
  }

  auto CheckAdditionalContextNotPresent() {
    return CheckJsResult(
        glic::kGlicContentsElementId,
        "() => { "
        "  let c = document.querySelector('#additionalContextResult');"
        "  return !c || c.children.length === 0;"
        "}",
        true);
  }

  auto CheckToastIsShowing(ToastId toast_id) {
    return PollUntil(
        [this, toast_id]() {
          auto* controller = browser()->GetFeatures().toast_controller();
          return controller && controller->IsShowingToast() &&
                 controller->GetCurrentToastId() == toast_id;
        },
        "polling until toast is showing");
  }

  auto WaitForShareResult(glic::ShareImageResult result) {
    auto steps = PollUntil(
        [this, result]() {
          return histogram_tester_.GetBucketCount(
                     "Glic.TabContext.ShareImageResult",
                     static_cast<int>(result)) == 1;
        },
        "polling until result");
    for (auto& step : steps) {
      step.SetMustRemainVisible(false);
    }
    return steps;
  }

 protected:
  base::HistogramTester histogram_tester_;

  static constexpr char kPageWithImage[] = "/test_visual.html";
  static constexpr char kPageWithUnsupportedImage[] =
      "/glic/page_with_simple_svg.html";

  glic::InstanceId cached_instance_id_;
};

class GlicInteractiveContextMenuTest
    : public GlicInteractiveContextMenuTestBase {
 public:
  GlicInteractiveContextMenuTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicShareImage},
        /*disabled_features=*/{features::kGlicWarming,
                               blink::features::kSvgFallBackToContainerSize});
    // Ensure that we open the FRE.
    glic_test_environment().SetFreStatusForNewProfiles(std::nullopt);
  }
  ~GlicInteractiveContextMenuTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInteractiveContextMenuTest, GlicShareImage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url = embedded_test_server()->GetURL(kPageWithImage);
  const DeepQuery kPathToImg{"img"};
  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url),
      WaitForWebContentsPainted(kActiveTab),
      MoveMouseTo(kActiveTab, kPathToImg), ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollForAndCompleteOnboarding(), PollForAndInstrumentGlic(),
      WaitForAdditionalContext(),
      WaitForShareResult(glic::ShareImageResult::kSentImageToClient),
      CheckHistograms());
}

IN_PROC_BROWSER_TEST_F(GlicInteractiveContextMenuTest, CreateNewInstance) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url = embedded_test_server()->GetURL(kPageWithImage);
  const DeepQuery kPathToImg{
      "img",
  };
  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url),
      WaitForWebContentsPainted(kActiveTab),
      ToggleGlicWindow(GlicWindowMode::kAttached),
      PollForAndCompleteOnboarding(),
      WaitForAndInstrumentGlic(kHostAndContents), CacheCurrentInstance(),
      MoveMouseTo(kActiveTab, kPathToImg), ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollForNewGlicInstance(), PollForAndInstrumentGlic(),
      WaitForAdditionalContext(),
      WaitForShareResult(glic::ShareImageResult::kSentImageToClient),
      CheckCachedInstance(), CheckHistograms());
}

// Disabled because flaky: crbug.com/519961669
IN_PROC_BROWSER_TEST_F(GlicInteractiveContextMenuTest,
                       DISABLED_CreateNewInstanceDetached) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url = embedded_test_server()->GetURL(kPageWithImage);
  const DeepQuery kPathToImg{
      "img",
  };
  // It appears that the detached window can set atop the image, interfering
  // with the context menu. If we change the window bounds, this is avoided.
  browser()->GetWindow()->SetBounds(gfx::Rect(0, 0, 1000, 1000));
  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url),
      WaitForWebContentsPainted(kActiveTab),
      ToggleGlicWindow(GlicWindowMode::kAttached),
      PollForAndCompleteOnboarding(),
      // In this case, we will close the detached panel and then open again in
      // side panel. This should still result in a new instance.
      WaitForAndInstrumentGlic(kHostAndContents), Detach(),
      CacheCurrentInstance(), MoveMouseTo(kActiveTab, kPathToImg),
      ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollForNewGlicInstance(), PollForAndInstrumentGlic(),
      WaitForAdditionalContext(),
      WaitForShareResult(glic::ShareImageResult::kSentImageToClient),
      CheckCachedInstance(), CheckHistograms());
}

IN_PROC_BROWSER_TEST_F(GlicInteractiveContextMenuTest,
                       GlicShareImageFailsOnNoImage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url = embedded_test_server()->GetURL(kPageWithUnsupportedImage);
  const DeepQuery kPathToImg{"img"};
  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url), MoveMouseTo(kActiveTab, kPathToImg),
      ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      CheckToastIsShowing(ToastId::kGlicShareImageFailed),
      WaitForShareResult(glic::ShareImageResult::kFailedNoImage));
}

class GlicTrustFirstOnboardingContextMenuTest
    : public GlicInteractiveContextMenuTestBase {
 public:
  GlicTrustFirstOnboardingContextMenuTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicShareImage},
        {features::kGlicWarming, blink::features::kSvgFallBackToContainerSize});
    glic_test_environment().SetFreStatusForNewProfiles(
        glic::prefs::FreStatus::kNotStarted);
  }
  ~GlicTrustFirstOnboardingContextMenuTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicTrustFirstOnboardingContextMenuTest,
                       GlicShareImageArm2) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const GURL url = embedded_https_test_server().GetURL(kPageWithImage);
  const DeepQuery kPathToImg{"img"};
  RunTestSequence(
      InstrumentTab(kActiveTab, std::nullopt, browser(), true),
      NavigateWebContents(kActiveTab, url),
      WaitForElementVisible(kActiveTab, kPathToImg),
      MoveMouseTo(kActiveTab, kPathToImg), ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(RenderViewContextMenu::kGlicShareImageMenuItem),
      PollForAndInstrumentGlic(), WaitForAdditionalContext(),
      PollForAndCompleteOnboarding());
}

}  // namespace
