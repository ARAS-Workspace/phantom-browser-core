// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/crx_file/id_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/performance_manager/public/features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/unpacked_installer.h"
#include "extensions/common/extension_urls.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/ui/browser_commands_mac.h"
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabPageElementId);

#if BUILDFLAG(IS_MAC)
bool kTestDisabledForVirtualMachineMac =
    (base::mac::MacOSMajorVersion() == 15) && base::mac::IsVirtualMachine();
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

class AppMenuModelInteractiveTest : public InteractiveBrowserTest {
 public:
  AppMenuModelInteractiveTest() = default;
  ~AppMenuModelInteractiveTest() override = default;
  AppMenuModelInteractiveTest(const AppMenuModelInteractiveTest&) = delete;
  void operator=(const AppMenuModelInteractiveTest&) = delete;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  auto CheckIncognitoWindowOpened(const Browser* default_browser) {
    return Check(base::BindLambdaForTesting([default_browser]() {
      BrowserWindowInterface* new_browser = nullptr;
      if (GlobalBrowserCollection::GetInstance()->GetIncognitoBrowserCount() ==
          1) {
        EXPECT_EQ(2u, GlobalBrowserCollection::GetInstance()->GetSize());
        ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
            [default_browser, &new_browser](BrowserWindowInterface* browser) {
              if (browser != default_browser) {
                new_browser = browser;
              }
              return !new_browser;
            });
        CHECK(new_browser);
      } else {
        new_browser = ui_test_utils::WaitForBrowserToOpen();
      }
      return new_browser->GetProfile()->IsIncognitoProfile();
    }));
  }

  auto CheckGuestWindowOpened(const Browser* default_browser) {
    return Check(base::BindLambdaForTesting([default_browser]() {
      BrowserWindowInterface* new_browser = nullptr;
      if (GlobalBrowserCollection::GetInstance()->GetGuestBrowserCount() == 1) {
        EXPECT_EQ(2u, GlobalBrowserCollection::GetInstance()->GetSize());
        ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
            [default_browser, &new_browser](BrowserWindowInterface* browser) {
              if (browser != default_browser) {
                new_browser = browser;
              }
              return !new_browser;
            });
        CHECK(new_browser);
      } else {
        new_browser = ui_test_utils::WaitForBrowserToOpen();
      }
      return new_browser->GetProfile()->IsGuestSession();
    }));
  }
};

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, PerformanceNavigation) {
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      ScrollIntoView(AppMenuModel::kMoreToolsMenuItem),
      SelectMenuItem(AppMenuModel::kMoreToolsMenuItem),
      ScrollIntoView(ToolsMenuModel::kPerformanceMenuItem),
      SelectMenuItem(ToolsMenuModel::kPerformanceMenuItem),
      WaitForWebContentsNavigation(
          kPrimaryTabPageElementId,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))));
}

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, IncognitoMenuItem) {
  RunTestSequence(PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kIncognitoMenuItem),
                  CheckIncognitoWindowOpened(browser()));
}

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, IncognitoAccelerator) {
  ui::Accelerator incognito_accelerator;
  AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
      IDC_NEW_INCOGNITO_WINDOW, &incognito_accelerator);

  RunTestSequence(
      SendAccelerator(kToolbarAppMenuButtonElementId, incognito_accelerator),
      CheckIncognitoWindowOpened(browser()));
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest,
                       ShowAppMenuInImmersiveFullscreen) {
  chrome::SetAlwaysShowToolbarInFullscreenForTesting(browser(), false);
  ASSERT_TRUE(!fullscreen_utils::IsAlwaysShowToolbarEnabled(browser()));
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  chrome::RevealToolbarForTesting(browser());
  RunTestSequence(WaitForShow(kToolbarAppMenuButtonElementId),
                  PressButton(kToolbarAppMenuButtonElementId),
                  WaitForShow(AppMenuModel::kMoreToolsMenuItem));
}
#endif  // BUILDFLAG(IS_MAC)

namespace {

enum ExtensionsTestMode {
  kDoNotCollapse,
  kCollapseNoExtensions,
  kCollapseWithExtensions
};

}  // namespace

class AppMenuModelExtensionsInteractiveTest
    : public AppMenuModelInteractiveTest,
      public testing::WithParamInterface<ExtensionsTestMode> {
 public:
  AppMenuModelExtensionsInteractiveTest() = default;
  ~AppMenuModelExtensionsInteractiveTest() override = default;

  bool MenuShouldCollapse() const {
    return GetParam() == ExtensionsTestMode::kCollapseNoExtensions;
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kExtensionsCollapseMainMenu,
        GetParam() != ExtensionsTestMode::kDoNotCollapse);
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    if (GetParam() != ExtensionsTestMode::kDoNotCollapse) {
      // Enable promotions.
      promotions_enabled_value_to_restore_opt_ =
          g_browser_process->local_state()->GetBoolean(
              prefs::kPromotionsEnabled);
      g_browser_process->local_state()->SetBoolean(prefs::kPromotionsEnabled,
                                                   true);
    }
    if (GetParam() == ExtensionsTestMode::kCollapseWithExtensions) {
      // Create and load a dummy extension.
      constexpr char kExtensionManifest[] = R"(
        {
          "name": "an extension",
          "version": "1.0",
          "manifest_version": 3,
          "action": {}
        }
      )";
      extensions::TestExtensionDir dir;
      dir.WriteManifest(kExtensionManifest);
      const auto id = crx_file::id_util::GenerateIdForPath(
          base::MakeAbsoluteFilePath(dir.UnpackedPath()));
      auto* const registry =
          extensions::ExtensionRegistry::Get(browser()->GetProfile());
      CHECK(registry);
      extensions::TestExtensionRegistryObserver observer(registry, id);
      extensions::UnpackedInstaller::Create(browser()->GetProfile())
          ->Load(dir.UnpackedPath());
      observer.WaitForExtensionLoaded();
    }
    AppMenuModelInteractiveTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    InteractiveBrowserTest::TearDownOnMainThread();
    if (promotions_enabled_value_to_restore_opt_.has_value()) {
      g_browser_process->local_state()->SetBoolean(
          prefs::kPromotionsEnabled,
          promotions_enabled_value_to_restore_opt_.value());
    }
  }

 protected:
  base::HistogramTester histograms_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::optional<bool> promotions_enabled_value_to_restore_opt_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    AppMenuModelExtensionsInteractiveTest,
    testing::Values(ExtensionsTestMode::kDoNotCollapse,
                    ExtensionsTestMode::kCollapseNoExtensions,
                    ExtensionsTestMode::kCollapseWithExtensions),
    [](const testing::TestParamInfo<ExtensionsTestMode>& param) {
      switch (param.param) {
        case ExtensionsTestMode::kDoNotCollapse:
          return "DoNotCollapse";
        case ExtensionsTestMode::kCollapseNoExtensions:
          return "CollapseNoExtensions";
        case ExtensionsTestMode::kCollapseWithExtensions:
          return "CollapseWithExtensions";
      }
    });

// Test to confirm that the manage extensions menu item navigates when selected
// and emit histograms that it did so.
IN_PROC_BROWSER_TEST_P(AppMenuModelExtensionsInteractiveTest,
                       ManageExtensions) {
  if (MenuShouldCollapse()) {
    GTEST_SKIP()
        << "Manage extensions cannot be accessed through collapsed menu.";
  }

  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kExtensionsMenuItem),
      SelectMenuItem(ExtensionsMenuModel::kManageExtensionsMenuItem),
      WaitForWebContentsNavigation(kPrimaryTabPageElementId,
                                   GURL(chrome::kChromeUIExtensionsURL)));

  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.ManageExtensions", 1);
  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.VisitChromeWebStore",
                               0);
  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.FindExtensions", 0);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_MANAGE_EXTENSIONS, 1);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_VISIT_CHROME_WEB_STORE, 0);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_FIND_EXTENSIONS, 0);
}

// Test to confirm that the visit Chrome Web Store menu item navigates to the
// correct chrome webstore URL when selected and emits histograms that it did
// so.
IN_PROC_BROWSER_TEST_P(AppMenuModelExtensionsInteractiveTest,
                       VisitChromeWebStore) {
  const bool collapse = MenuShouldCollapse();
  const GURL expected_webstore_launch_url =
      extension_urls::GetNewWebstoreLaunchURL();
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      // If not collapsed, then the web store item is in the extensions submenu.
      If([collapse]() { return !collapse; },
         Then(SelectMenuItem(AppMenuModel::kExtensionsMenuItem))),
      SelectMenuItem(ExtensionsMenuModel::kVisitChromeWebStoreMenuItem),
      WaitForWebContentsNavigation(
          kPrimaryTabPageElementId,
          extension_urls::AppendUtmSource(expected_webstore_launch_url,
                                          extension_urls::kAppMenuUtmSource)));

  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.VisitChromeWebStore",
                               collapse ? 0 : 1);
  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.FindExtensions",
                               collapse ? 1 : 0);
  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.ManageExtensions", 0);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_VISIT_CHROME_WEB_STORE,
                                collapse ? 0 : 1);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_FIND_EXTENSIONS, collapse ? 1 : 0);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_MANAGE_EXTENSIONS, 0);
}

class PasswordManagerMenuItemInteractiveTest
    : public AppMenuModelInteractiveTest,
      public testing::WithParamInterface<bool> {
 public:
  PasswordManagerMenuItemInteractiveTest() = default;
  PasswordManagerMenuItemInteractiveTest(
      const PasswordManagerMenuItemInteractiveTest&) = delete;
  void operator=(const PasswordManagerMenuItemInteractiveTest&) = delete;

  ~PasswordManagerMenuItemInteractiveTest() override = default;
};

IN_PROC_BROWSER_TEST_F(PasswordManagerMenuItemInteractiveTest,
                       PasswordManagerMenuItem) {
  base::HistogramTester histograms;

  RunTestSequence(InstrumentTab(kPrimaryTabPageElementId),
                  PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kPasswordAndAutofillMenuItem),
                  SelectMenuItem(AppMenuModel::kPasswordManagerMenuItem),
                  WaitForWebContentsNavigation(
                      kPrimaryTabPageElementId,
                      GURL("chrome://password-manager/passwords")));

  histograms.ExpectTotalCount("WrenchMenu.TimeToAction.ShowPasswordManager", 1);
  histograms.ExpectBucketCount("WrenchMenu.MenuAction",
                               MENU_ACTION_SHOW_PASSWORD_MANAGER, 1);
}

IN_PROC_BROWSER_TEST_F(PasswordManagerMenuItemInteractiveTest,
                       NoMenuItemOnPasswordManagerPage) {
  RunTestSequence(
      AddInstrumentedTab(kPrimaryTabPageElementId,
                         GURL("chrome://password-manager/passwords")),
      WaitForWebContentsReady(kPrimaryTabPageElementId,
                              GURL("chrome://password-manager/passwords")),
      PressButton(kToolbarAppMenuButtonElementId),
      EnsureNotPresent(AppMenuModel::kPasswordManagerMenuItem));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
class SupervisedUserAppMenuModelInteractiveTest
    : public AppMenuModelInteractiveTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    unused_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  // Required to use IdentityTestEnvironmentAdaptor.
                  IdentityTestEnvironmentProfileAdaptor::
                      SetIdentityTestEnvironmentFactoriesOnBrowserContext(
                          context);
                }));
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->GetProfile());
  }

  void SignIn(bool is_supervised_user) {
    AccountInfo account_info =
        identity_test_environment_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("name@gmail.com",
                                          signin::ConsentLevel::kSignin);
    supervised_user::UpdateSupervisionStatusForAccount(
        account_info,
        identity_test_environment_adaptor_->identity_test_env()
            ->identity_manager(),
        is_supervised_user);
  }

 private:
  base::CallbackListSubscription unused_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
};

IN_PROC_BROWSER_TEST_F(SupervisedUserAppMenuModelInteractiveTest,
                       OpenGuestSessionForSignedOutUser) {
  RunTestSequence(PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kProfileMenuItem),
                  SelectMenuItem(AppMenuModel::kProfileOpenGuestItem),
                  CheckGuestWindowOpened(browser()));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserAppMenuModelInteractiveTest,
                       OpenGuestSessionForSignedInRegularUser) {
  SignIn(/*is_supervised_user=*/false);
  RunTestSequence(PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kProfileMenuItem),
                  SelectMenuItem(AppMenuModel::kProfileOpenGuestItem),
                  CheckGuestWindowOpened(browser()));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserAppMenuModelInteractiveTest,
                       OpenGuestSessionForSignedInSupervisedUser) {
  SignIn(/*is_supervised_user=*/true);

  RunTestSequence(PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kProfileMenuItem),
                  EnsureNotPresent(AppMenuModel::kProfileOpenGuestItem));
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
