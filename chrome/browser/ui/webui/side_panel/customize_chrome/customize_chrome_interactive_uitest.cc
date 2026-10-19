// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/actions/actions.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);

class CustomizeChromeInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter}, {});
    InteractiveBrowserTest::SetUp();
  }

  InteractiveTestApi::MultiStep ClickElement(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    return Steps(WaitForElementToRender(contents_id, element),
                 ExecuteJsAt(contents_id, element, "el => el.click()"));
  }

  InteractiveTestApi::MultiStep WaitForElementExists(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    StateChange element_exists;
    element_exists.type =
        WebContentsInteractionTestUtil::StateChange::Type::kExists;
    element_exists.event = kElementExists;
    element_exists.where = element;
    return WaitForStateChange(contents_id, element_exists);
  }

  InteractiveTestApi::MultiStep OpenCustomizeChromeSidePanel(
      const ui::ElementIdentifier& contents_id) {
    return Steps(
        Do(base::BindLambdaForTesting([=, this]() {
          chrome::ExecuteCommandWithContext(
              browser(), IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL,
              actions::ActionInvocationContext::Builder()
                  .SetProperty(
                      kSidePanelOpenTriggerKey,
                      static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                          SidePanelOpenTrigger::kToolbarButton))
                  .Build());
        })),
        WaitForShow(kCustomizeChromeSidePanelWebViewElementId),
        InstrumentNonTabWebView(contents_id,
                                kCustomizeChromeSidePanelWebViewElementId));
  }

  InteractiveTestApi::MultiStep CheckFooterToggleState(
      const ui::ElementIdentifier& contents_id,
      bool enabled) {
    const DeepQuery kFooterToggle = {"customize-chrome-app", "#footer",
                                     "customize-chrome-footer", "#showToggle"};
    const std::string enabled_js_result =
        enabled ? "el => !el.disabled" : "el => el.disabled";
    return Steps(
        WaitForElementToRender(contents_id, kFooterToggle),
        CheckJsResultAt(contents_id, kFooterToggle, enabled_js_result),
        CheckJsResultAt(contents_id, kFooterToggle, "el => el.checked"));
  }

  // Loads an extensions overriding the NTP. `index` is used to differentiate
  // multiple installed extensions.
  void LoadNtpOverridingExtension(int index = 0) {
    extensions::TestExtensionDir extension_dir;
    extension_dir.WriteFile(FILE_PATH_LITERAL("ext.html"),
                            "<body>Extension-overridden NTP</body>");

    const char extension_manifest[] = R"(
        {
            "chrome_url_overrides": {
                "newtab": "ext.html"
            },
            "name": "Extension-overridden NTP %d",
            "manifest_version": 3,
            "version": "0.1"
          })";

    extension_dir.WriteManifest(absl::StrFormat(extension_manifest, index));

    extensions::ChromeTestExtensionLoader extension_loader(
        browser()->GetProfile());
    extension_loader.set_ignore_manifest_warnings(true);
    const extensions::Extension* extension =
        extension_loader.LoadExtension(extension_dir.Pack()).get();
    ASSERT_TRUE(extension);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(CustomizeChromeInteractiveTest,
                       ShowsFooterSectionForExtensionNtp) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLocalCustomizeChromeElementId);
  const DeepQuery kFooterSection = {"customize-chrome-app", "#footer",
                                    "customize-chrome-footer",
                                    "#showToggleContainer"};
  // Load extension that overrides NTP.
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension new tab page.
      AddInstrumentedTab(kNewTabElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Open customize chrome side panel.
      OpenCustomizeChromeSidePanel(kLocalCustomizeChromeElementId),
      // Check that the footer section exists.
      Steps(
          WaitForElementExists(kLocalCustomizeChromeElementId, kFooterSection),
          WaitForElementToRender(kLocalCustomizeChromeElementId,
                                 kFooterSection)));
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeInteractiveTest,
                       FooterSectionNotShownForNonExtensionNtp) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLocalCustomizeChromeElementId);
  const DeepQuery kFooterSection = {"customize-chrome-app", "#footer",
                                    "customize-chrome-footer",
                                    "#showToggleContainer"};
  RunTestSequence(
      // Open non-extension new tab page.
      AddInstrumentedTab(kNewTabElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Open customize chrome side panel.
      OpenCustomizeChromeSidePanel(kLocalCustomizeChromeElementId),
      // Check that the footer section does not exist.
      EnsureNotPresent(kLocalCustomizeChromeElementId, kFooterSection));
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeInteractiveTest,
                       TogglesFooterVisibility) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLocalCustomizeChromeElementId);
  const DeepQuery kFooterToggle = {"customize-chrome-app", "#footer",
                                   "customize-chrome-footer",
                                   "#showToggleContainer"};
  // Install extension NTP.
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open NTP with footer showing.
      Steps(AddInstrumentedTab(kNewTabElementId,
                               chrome::ChromeUINewTabURLAsGURL()),
            WaitForShow(kNtpFooterViewElementId)),
      // Click the footer toggle.
      Steps(OpenCustomizeChromeSidePanel(kLocalCustomizeChromeElementId),
            ClickElement(kLocalCustomizeChromeElementId, kFooterToggle)),
      // Ensure footer hides.
      WaitForHide(kNtpFooterViewElementId),
      // Click the footer toggle.
      ClickElement(kLocalCustomizeChromeElementId, kFooterToggle),
      // Ensure footer shows.
      WaitForShow(kNtpFooterViewElementId));
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class CustomizeChromeEnterpriseInteractiveTest
    : public CustomizeChromeInteractiveTest {
 public:
  void SetUpOnMainThread() override {
    scoped_browser_management_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(
                browser()->GetProfile()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    CustomizeChromeInteractiveTest::SetUpOnMainThread();
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter}, {});
    InteractiveBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override { scoped_browser_management_.reset(); }

  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_browser_management_;
};

IN_PROC_BROWSER_TEST_F(
    CustomizeChromeEnterpriseInteractiveTest,
    FooterToggle_ManagementNoticeForceEnabledWithCustomPolicy) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLocalCustomizeChromeElementId);

  RunTestSequence(
      // Set custom label policy for the management notice.
      Do(base::BindLambdaForTesting([=]() {
        g_browser_process->local_state()->SetString(
            prefs::kEnterpriseCustomLabelForBrowser, "Custom Label");
      })),
      // Open non-extension new tab page.
      AddInstrumentedTab(kNewTabElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Open customize chrome side panel.
      OpenCustomizeChromeSidePanel(kLocalCustomizeChromeElementId),
      // Check that the footer toggle is turned on but can't be toggled.
      CheckFooterToggleState(kLocalCustomizeChromeElementId,
                             /*enabled=*/false));
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeEnterpriseInteractiveTest,
                       FooterToggle_ExtensionAttributionPolicyChanges) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLocalCustomizeChromeElementId);
  const DeepQuery kFooterSection = {"customize-chrome-app", "#footer"};

  // Load extension that overrides NTP.
  LoadNtpOverridingExtension();
  RunTestSequence(
      // Open extension new tab page.
      AddInstrumentedTab(kNewTabElementId, chrome::ChromeUINewTabURLAsGURL()),
      // Open customize chrome side panel.
      OpenCustomizeChromeSidePanel(kLocalCustomizeChromeElementId),
      // Check that the footer toggle is turned on and can be toggled.
      CheckFooterToggleState(kLocalCustomizeChromeElementId,
                             /*enabled=*/true),
      // Disable extension attribution by policy.
      Do(base::BindLambdaForTesting([=, this]() {
        browser()->GetProfile()->GetPrefs()->SetBoolean(
            prefs::kNTPFooterExtensionAttributionEnabled, false);
      })),
      // Check that the footer section still exists.
      Steps(
          WaitForElementExists(kLocalCustomizeChromeElementId, kFooterSection),
          WaitForElementToRender(kLocalCustomizeChromeElementId,
                                 kFooterSection)));
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
