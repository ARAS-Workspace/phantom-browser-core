// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/version_info/version_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_restore_test_utils.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_init_state.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/ui/startup/profile_launch_observer.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/window_metadata/window_metadata_controller.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "base/json/json_string_value_serializer.h"
#include "chrome/browser/ui/views/web_apps/protocol_handler_launch_dialog_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#endif

using testing::Return;

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/chrome_browser_application_mac.h"
#endif

using extensions::Extension;
using testing::_;
using web_app::WebAppProvider;

namespace {

void DisableWhatsNewPage() {
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetInteger(prefs::kLastWhatsNewVersion,
                           version_info::GetMajorVersionNumberAsInt());
}

Browser* OpenNewBrowser(Profile* profile,
                        const std::optional<std::string>&
                            version_string_for_testing = std::nullopt) {
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl creator(base::FilePath(), dummy,
                                    chrome::startup::IsFirstRun::kYes);
  creator.SetCurrentChromeVersionStringForTesting(version_string_for_testing);
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  creator.Launch(profile, chrome::startup::IsProcessStartup::kNo,
                 /*restore_tabbed_browser=*/true);
  Browser* new_browser = browser_created_observer.Wait();
  ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);
  return new_browser;
}

bool HasInfoBar(infobars::ContentInfoBarManager* infobar_manager,
                const infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  return std::ranges::contains(infobar_manager->infobars(), identifier,
                               &infobars::InfoBar::GetIdentifier);
}

struct StartupBrowserCreatorFlagTypeValue {
  std::string flag;
  infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier;
  // True if the infobar is supposed to be shown in every tab, false if it is
  // only supposed to be shown once.
  bool is_global_infobar;
};

typedef std::optional<policy::PolicyLevel> PolicyVariant;

// Extra parts that facilitates observing the first Browser instance created
// for the chrome process.
class BrowserCreatedMainParts : public ChromeBrowserMainExtraParts,
                                public BrowserCollectionObserver {
 public:
  explicit BrowserCreatedMainParts(base::OnceClosure first_browser_created_cb)
      : first_browser_created_cb_(std::move(first_browser_created_cb)) {}
  BrowserCreatedMainParts(const BrowserCreatedMainParts&) = delete;
  BrowserCreatedMainParts& operator=(const BrowserCreatedMainParts&) = delete;
  ~BrowserCreatedMainParts() override = default;

  // ChromeBrowserMainExtraParts:
  void PreCreateThreads() override {
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
  }

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    std::move(first_browser_created_cb_).Run();
    browser_collection_observation_.Reset();
  }

 private:
  base::OnceClosure first_browser_created_cb_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

}  // namespace

class StartupBrowserCreatorTest : public extensions::ExtensionBrowserTest {
 protected:
  StartupBrowserCreatorTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kNonMilestoneUpdateToast);
  }

  bool SetUpUserDataDirectory() override {
    return extensions::ExtensionBrowserTest::SetUpUserDataDirectory();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kHomePage, url::kAboutBlankURL);
  }

  BrowserWindowInterface* FindOneOtherBrowserForProfile(
      Profile* profile,
      BrowserWindowInterface* not_this_browser) {
    BrowserWindowInterface* result = nullptr;
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [profile, not_this_browser, &result](BrowserWindowInterface* browser) {
          if (browser != not_this_browser && browser->GetProfile() == profile) {
            result = browser;
          }
          return !result;
        });
    return result;
  }

  // A helper function that checks the session restore UI (infobar) is shown
  // when Chrome starts up after crash.
  void EnsureRestoreUIWasShown(content::WebContents* web_contents) {
#if BUILDFLAG(IS_MAC)
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents);
    EXPECT_EQ(1U, infobar_manager->infobars().size());
#endif  // BUILDFLAG(IS_MAC)
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that when there is a popup as the active browser any requests to
// StartupBrowserCreatorImpl::OpenURLsInBrowser don't crash because there's no
// explicit profile given.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenURLsPopup) {
  std::vector<GURL> urls;
  urls.emplace_back("http://localhost");

  // Note that in our testing we do not ever query the BrowserList for the "last
  // active" browser. That's because the browsers are set as "active" by
  // platform UI toolkit messages, and those messages are not sent during unit
  // testing sessions.

  BrowserWindowInterface* popup = CreateBrowserWindow(BrowserWindowCreateParams(
      BrowserWindowInterface::TYPE_POPUP, browser()->GetProfile(),
      /*from_user_gesture=*/true));
  ASSERT_EQ(popup->GetType(), BrowserWindowInterface::Type::TYPE_POPUP);

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IsFirstRun::kYes
                                    : chrome::startup::IsFirstRun::kNo;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  // This should create a new window, but re-use the profile from |popup|. If
  // it used a null or invalid profile, it would crash.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  launch.OpenURLsInBrowser(popup->GetBrowserForMigrationOnly(),
                           chrome::startup::IsProcessStartup::kNo, urls);
  BrowserWindowInterface* created_browser = browser_created_observer.Wait();
  ASSERT_NE(popup, created_browser);
}

// Session restore for process-startup browser launches is tested in
// session_restore_uitest.
// Verify that startup URLs are honored when the process already exists but has
// no tabbed browser windows (eg. as if the process is running only due to a
// background application.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       StartupURLsOnNewWindowWithNoTabbedBrowsers) {
  // Use a couple same-site HTTP URLs.
  ASSERT_TRUE(embedded_test_server()->Start());
  std::vector<GURL> urls;
  urls.push_back(embedded_test_server()->GetURL("/title1.html"));
  urls.push_back(embedded_test_server()->GetURL("/title2.html"));

  Profile* profile = browser()->GetProfile();

  DisableWhatsNewPage();

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(profile, pref);

  // Keep the browser process running while browsers are closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  Browser* new_browser = OpenNewBrowser(profile);
  ASSERT_TRUE(new_browser);

  std::vector<GURL> expected_urls(urls);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(static_cast<int>(expected_urls.size()), tab_strip->count());
  for (size_t i = 0; i < expected_urls.size(); i++) {
    EXPECT_EQ(expected_urls[i],
              tab_strip->GetWebContentsAt(i)->GetVisibleURL());
  }

  // The two test_server tabs, despite having the same site, should be in
  // different SiteInstances.
  EXPECT_NE(
      tab_strip->GetWebContentsAt(tab_strip->count() - 2)->GetSiteInstance(),
      tab_strip->GetWebContentsAt(tab_strip->count() - 1)->GetSiteInstance());
}

// Verify that startup URLs aren't used when the process already exists
// and has other tabbed browser windows.  This is the common case of starting a
// new browser.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, StartupURLsOnNewWindow) {
  // Use a couple arbitrary URLs.
  std::vector<GURL> urls;
  urls.push_back(chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  urls.push_back(chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(browser()->GetProfile(), pref);

  DisableWhatsNewPage();

  Browser* new_browser = OpenNewBrowser(browser()->GetProfile());
  ASSERT_TRUE(new_browser);

  // The new browser should have exactly one tab (not the startup URLs).
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(chrome::ChromeUINewTabURLAsGURL(),
            tab_strip->GetWebContentsAt(0)->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ClearsProfileSetsAfterActivation) {
  Profile* profile = browser()->GetProfile();
  StartupBrowserCreator::ClearLaunchedProfilesForTesting();

  ProfileLaunchObserver* observer = ProfileLaunchObserver::GetInstance();
  ASSERT_TRUE(observer->launched_profiles_.empty());
  ASSERT_TRUE(observer->opened_profiles_.empty());

  StartupBrowserCreator::Profiles last_opened_profiles = {profile};
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  browser_creator.LaunchBrowserForLastProfiles(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      chrome::startup::IsFirstRun::kNo,
      {profile, StartupProfileMode::kBrowserWindow}, last_opened_profiles,
      /*restore_tabbed_browser=*/true);

  // Once activation has been scheduled, ProfileLaunchObserver has finished
  // waiting for launched profiles to open. Do not keep raw Profile pointers
  // around after it stops observing Profile destruction.
  EXPECT_TRUE(observer->launched_profiles_.empty());
  EXPECT_TRUE(observer->opened_profiles_.empty());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       KSameTabSwitchReplacesActiveTab) {
  // Use a couple of arbitrary URLs.
  std::vector<GURL> urls;
  urls.push_back(chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  urls.push_back(chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));
  urls.push_back(chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title3.html"))));

  DisableWhatsNewPage();

  // Open a browser window with some preloaded tabs.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://localhost"), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());  // Verify one tab is open.

  // Set the first tab as the active tab.
  tab_strip->ActivateTabAt(0);
  EXPECT_EQ(0, tab_strip->active_index());

  // Add the --kSameTab switch and URLs to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kSameTab);  // Add the switch.
  command_line.AppendArg(urls[0].spec());         // First URL.
  command_line.AppendArg(urls[1].spec());         // Second URL.
  command_line.AppendArg(urls[2].spec());         // Third URL.

  // Process the command line to simulate the launch.
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->GetProfile(), StartupProfileMode::kBrowserWindow}, {}));

  // Verify the behavior:
  // - The active tab's URL should be replaced by the first URL.
  EXPECT_EQ(urls[0], tab_strip->GetWebContentsAt(0)->GetVisibleURL());

  // - The remaining URLs should open in new tabs.
  EXPECT_EQ(3, tab_strip->count());  // Verify total tabs.
  EXPECT_EQ(urls[1], tab_strip->GetWebContentsAt(1)->GetVisibleURL());
  EXPECT_EQ(urls[2], tab_strip->GetWebContentsAt(2)->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, ShowNonMilestoneUpdateToast) {
  // Set pref to current Chrome version.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetString(
      prefs::kNonMilestoneUpdateToastVersion,
      base::StringPrintf("%d.%d.%d.%d", CHROME_VERSION_MAJOR,
                         CHROME_VERSION_MINOR, CHROME_VERSION_BUILD,
                         CHROME_VERSION_PATCH));

  // Set current chrome version to next non milestone update version.
  std::string chrome_version_string_for_testing = base::StringPrintf(
      "%d.%d.%d.%d", CHROME_VERSION_MAJOR, CHROME_VERSION_MINOR,
      CHROME_VERSION_BUILD, CHROME_VERSION_PATCH + 1);

  // Open a new browser and verify the toast is shown.
  Browser* new_browser = OpenNewBrowser(browser()->GetProfile(),
                                        chrome_version_string_for_testing);
  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(
      new_browser->GetFeatures().toast_controller()->GetToastViewForTesting());

  // Open another new browser and verify the toast is not shown.
  Browser* new_browser2 = OpenNewBrowser(browser()->GetProfile(),
                                         chrome_version_string_for_testing);
  ASSERT_TRUE(new_browser2);
  ASSERT_FALSE(
      new_browser2->GetFeatures().toast_controller()->GetToastViewForTesting());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       StartupWithValidWindowNameUTF8Works) {
  // Suffixes are e-acute, and e-acute followed by "100" red emoji.
  std::vector<std::string> test_names = {"touche", "touch\xC3\xA9",
                                         "touch\xC3\xA9\xF0\x9F\x92\xAF"};

  for (const auto& expected_name : test_names) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(switches::kOpenInNewWindow);
    command_line.AppendSwitchNative(switches::kWindowName, expected_name);

    ui_test_utils::BrowserCreatedObserver browser_created_observer;

    ASSERT_TRUE(StartupBrowserCreator().Start(
        command_line, base::FilePath(),
        {browser()->GetProfile(), StartupProfileMode::kBrowserWindow}, {}));

    Browser* new_browser = browser_created_observer.Wait();
    ASSERT_TRUE(new_browser);
    EXPECT_EQ(expected_name,
              WindowMetadataController::From(new_browser)->user_title());

    CloseBrowserSynchronously(new_browser);
  }
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       StartupWithInvalidWindowNameUTF8LeadsToDefault) {
  // Test invalid UTF-8 sequence: should not fail startup, and window should act
  // as if name empty.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kOpenInNewWindow);
  std::string invalid_utf8 = "\xFF\xFF";
  command_line.AppendSwitchNative(switches::kWindowName, invalid_utf8);

  ui_test_utils::BrowserCreatedObserver browser_created_observer;

  ASSERT_TRUE(StartupBrowserCreator().Start(
      command_line, base::FilePath(),
      {browser()->GetProfile(), StartupProfileMode::kBrowserWindow}, {}));

  Browser* new_browser = browser_created_observer.Wait();
  ASSERT_TRUE(new_browser);
  // On POSIX/Linux, the invalid UTF-8 is strictly rejected, falling back to "".
  EXPECT_EQ("", WindowMetadataController::From(new_browser)->user_title());

  CloseBrowserSynchronously(new_browser);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ReadingWasRestartedAfterRestart) {
  // Tests that StartupBrowserCreator::WasRestarted reads and resets the
  // preference kWasRestarted correctly.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);
  EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ReadingWasRestartedAfterNormalStart) {
  // Tests that StartupBrowserCreator::WasRestarted reads and resets the
  // preference kWasRestarted correctly.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, false);
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
}

// If startup pref is set as LAST_AND_URLS, startup urls should be opened in a
// new browser window separated from the last-session-restored browser.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, StartupPrefSetAsLastAndURLs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create a new profile.
  base::FilePath dest_path =
      profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("New Profile"));
  Profile& profile =
      profiles::testing::CreateProfileSync(profile_manager, dest_path);

  DisableWhatsNewPage();

  const GURL t1_url = embedded_test_server()->GetURL("/title1.html");
  const GURL t2_url = embedded_test_server()->GetURL("/title2.html");
  const GURL t3_url = embedded_test_server()->GetURL("/title3.html");

  // Set the profiles to open both urls and last visited pages.
  SessionStartupPref startup_pref(SessionStartupPref::LAST_AND_URLS);
  std::vector<GURL> urls_to_open;
  urls_to_open.push_back(t1_url);
  urls_to_open.push_back(t2_url);
  startup_pref.urls = urls_to_open;
  SessionStartupPref::SetStartupPref(&profile, startup_pref);

  // Open |t3_url| in a tab.
  Browser* new_browser =
      CreateBrowserWindow(BrowserWindowCreateParams(
                              BrowserWindowInterface::TYPE_NORMAL, &profile,
                              /*from_user_gesture=*/true))
          ->GetBrowserForMigrationOnly();
  TabStripModel* tab_strip_model = new_browser->tab_strip_model();
  ui_test_utils::NavigateToURLWithDisposition(
      new_browser, t3_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(t3_url,
            tab_strip_model->GetWebContentsAt(0)->GetLastCommittedURL());

  // Close the browser without deleting |profile|.
  ScopedProfileKeepAlive profile_keep_alive(
      &profile, ProfileKeepAliveOrigin::kBrowserWindow);
  CloseBrowserSynchronously(new_browser);

  // Close the main browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(browser()->GetProfile());
  last_opened_profiles.push_back(&profile);

  base::RunLoop run_loop;
  browser_creator.Start(
      dummy, profile_manager->user_data_dir(),
      {browser()->GetProfile(), StartupProfileMode::kBrowserWindow},
      last_opened_profiles);
  testing::SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 1);
  run_loop.Run();

  const auto wait_for_load_stop_for_browser =
      [](BrowserWindowInterface* browser) {
        TabStripModel* const tab_strip_model = browser->GetTabStripModel();
        for (int i = 0; i < tab_strip_model->count(); ++i) {
          content::WebContents* const contents =
              tab_strip_model->GetWebContentsAt(i);
          EXPECT_TRUE(content::WaitForLoadStop(contents));
        }
      };

  // |profile| restored the last open pages and opened the urls in an active new
  // window.
  ASSERT_EQ(2u, ProfileBrowserCollection::GetForProfile(&profile)->GetSize());

  std::vector<BrowserWindowInterface*> profile_browsers;
  ProfileBrowserCollection::GetForProfile(&profile)->ForEach(
      [&profile_browsers](BrowserWindowInterface* browser) {
        profile_browsers.push_back(browser);
        return true;
      });

  BrowserWindowInterface* const last_session_opened_browser =
      profile_browsers[0];
  ASSERT_TRUE(last_session_opened_browser);
  BrowserWindowInterface* const pref_urls_opened_browser = profile_browsers[1];
  ASSERT_TRUE(pref_urls_opened_browser);
  // Check the last-session-restored browser.
  EXPECT_NO_FATAL_FAILURE(
      wait_for_load_stop_for_browser(last_session_opened_browser));
  tab_strip_model = last_session_opened_browser->GetTabStripModel();
  ASSERT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(t3_url, tab_strip_model->GetWebContentsAt(0)->GetVisibleURL());
  // Check the pref-urls-opened browser.
  EXPECT_NO_FATAL_FAILURE(
      wait_for_load_stop_for_browser(pref_urls_opened_browser));
  tab_strip_model = pref_urls_opened_browser->GetTabStripModel();
  EXPECT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(t1_url, tab_strip_model->GetWebContentsAt(0)->GetVisibleURL());
  EXPECT_EQ(t2_url, tab_strip_model->GetWebContentsAt(1)->GetVisibleURL());
  EXPECT_EQ(0, tab_strip_model->active_index());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, StartupURLsForTwoProfiles) {
  Profile* default_profile = browser()->GetProfile();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Create another profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile 1"));
  Profile& other_profile =
      profiles::testing::CreateProfileSync(profile_manager, dest_path);

  // Use a couple arbitrary URLs.
  std::vector<GURL> urls1;
  urls1.push_back(chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set different startup preferences for the 2 profiles.
  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(default_profile, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(&other_profile, pref2);

  DisableWhatsNewPage();

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(default_profile);
  last_opened_profiles.push_back(&other_profile);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        {default_profile, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);

  // urls1 were opened in a browser for default_profile, and urls2 were opened
  // in a browser for other_profile.
  BrowserWindowInterface* new_browser =
      FindOneOtherBrowserForProfile(default_profile, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->GetTabStripModel();

  // The new browser should have only the desired URL for the profile.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls1[0], tab_strip->GetWebContentsAt(0)->GetVisibleURL());

  ASSERT_EQ(1u,
            ProfileBrowserCollection::GetForProfile(&other_profile)->GetSize());
  new_browser = FindOneOtherBrowserForProfile(&other_profile, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->GetTabStripModel();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls2[0], tab_strip->GetWebContentsAt(0)->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, PRE_UpdateWithTwoProfiles) {
  // Simulate a browser restart by creating the profiles in the PRE_ part.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Create two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
  DisableWhatsNewPage();

  // Don't delete Profiles too early.
  ScopedProfileKeepAlive profile1_keep_alive(
      &profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      &profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open some urls with the browsers, and close them.
  Browser* browser1 =
      CreateBrowserWindow(BrowserWindowCreateParams(
                              BrowserWindowInterface::TYPE_NORMAL, &profile1,
                              /*from_user_gesture=*/true))
          ->GetBrowserForMigrationOnly();
  chrome::NewTab(browser1, NewTabTypes::kNoUserAction);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/empty.html")));
  CloseBrowserSynchronously(browser1);

  Browser* browser2 =
      CreateBrowserWindow(BrowserWindowCreateParams(
                              BrowserWindowInterface::TYPE_NORMAL, &profile2,
                              /*from_user_gesture=*/true))
          ->GetBrowserForMigrationOnly();
  chrome::NewTab(browser2, NewTabTypes::kNoUserAction);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/form.html")));
  CloseBrowserSynchronously(browser2);

  // Set different startup preferences for the 2 profiles.
  std::vector<GURL> urls1;
  urls1.push_back(chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set different startup preferences for the 2 profiles.
  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(&profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(&profile2, pref2);

  profile1.GetPrefs()->CommitPendingWrite();
  profile2.GetPrefs()->CommitPendingWrite();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, UpdateWithTwoProfiles) {
  // Make StartupBrowserCreator::WasRestarted() return true.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));

  // Simulate a launch after a browser update.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(&profile1);
  last_opened_profiles.push_back(&profile2);

  base::RunLoop run_loop;
  testing::SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 2);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        {&profile1, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);
  run_loop.Run();

  // The startup URLs are ignored, and instead the last open sessions are
  // restored.
  EXPECT_TRUE(profile1.restored_last_session());
  EXPECT_TRUE(profile2.restored_last_session());

  BrowserWindowInterface* new_browser = nullptr;
  ASSERT_EQ(1u, ProfileBrowserCollection::GetForProfile(&profile1)->GetSize());
  new_browser = FindOneOtherBrowserForProfile(&profile1, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->GetTabStripModel();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().GetPath());

  ASSERT_EQ(1u, ProfileBrowserCollection::GetForProfile(&profile2)->GetSize());
  new_browser = FindOneOtherBrowserForProfile(&profile2, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->GetTabStripModel();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/form.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().GetPath());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ProfilesWithoutPagesNotLaunched) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 4 more profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  base::FilePath dest_path3 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 3"));
  base::FilePath dest_path4 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 4"));

  Profile& profile_home1 =
      profiles::testing::CreateProfileSync(profile_manager, dest_path1);
  Profile& profile_home2 =
      profiles::testing::CreateProfileSync(profile_manager, dest_path2);
  Profile& profile_last =
      profiles::testing::CreateProfileSync(profile_manager, dest_path3);
  Profile& profile_urls =
      profiles::testing::CreateProfileSync(profile_manager, dest_path4);

  DisableWhatsNewPage();

  // Set the profiles to open urls, open last visited pages or display the home
  // page.
  SessionStartupPref pref_home(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(&profile_home1, pref_home);
  SessionStartupPref::SetStartupPref(&profile_home2, pref_home);

  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(&profile_last, pref_last);

  std::vector<GURL> urls;
  urls.push_back(chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls = urls;
  SessionStartupPref::SetStartupPref(&profile_urls, pref_urls);

  // Open a page with profile_last.
  Browser* browser_last =
      CreateBrowserWindow(
          BrowserWindowCreateParams(BrowserWindowInterface::TYPE_NORMAL,
                                    &profile_last, /*from_user_gesture=*/true))
          ->GetBrowserForMigrationOnly();
  chrome::NewTab(browser_last, NewTabTypes::kNoUserAction);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser_last, embedded_test_server()->GetURL("/empty.html")));

  // Close the browser without deleting |profile_last|.
  ScopedProfileKeepAlive profile_last_keep_alive(
      &profile_last, ProfileKeepAliveOrigin::kBrowserWindow);
  CloseBrowserSynchronously(browser_last);

  // Close the main browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(&profile_home1);
  last_opened_profiles.push_back(&profile_home2);
  last_opened_profiles.push_back(&profile_last);
  last_opened_profiles.push_back(&profile_urls);

  base::RunLoop run_loop;
  // Only profile_last should get its session restored.
  testing::SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 1);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        {&profile_home1, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);
  run_loop.Run();

  BrowserWindowInterface* new_browser = nullptr;
  // The last open profile (the profile_home1 in this case) will always be
  // launched, even if it will open just the NTP.
  ASSERT_EQ(1u,
            ProfileBrowserCollection::GetForProfile(&profile_home1)->GetSize());
  new_browser = FindOneOtherBrowserForProfile(&profile_home1, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->GetTabStripModel();

  // The new browser should have only the NTP.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(ntp_test_utils::GetFinalNtpUrl(new_browser->GetProfile()),
            tab_strip->GetWebContentsAt(0)->GetVisibleURL());

  // profile_urls opened the urls.
  ASSERT_EQ(1u,
            ProfileBrowserCollection::GetForProfile(&profile_urls)->GetSize());
  new_browser = FindOneOtherBrowserForProfile(&profile_urls, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->GetTabStripModel();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls[0], tab_strip->GetWebContentsAt(0)->GetVisibleURL());

  // profile_last opened the last open pages.
  ASSERT_EQ(1u,
            ProfileBrowserCollection::GetForProfile(&profile_last)->GetSize());
  new_browser = FindOneOtherBrowserForProfile(&profile_last, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->GetTabStripModel();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().GetPath());

  // profile_home2 was not launched since it would've only opened the home page.
  ASSERT_EQ(0u,
            ProfileBrowserCollection::GetForProfile(&profile_home2)->GetSize());
}

// This tests that opening multiple profiles with session restore enabled,
// shutting down, and then launching with kNoStartupWindow doesn't restore
// the previously opened profiles.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, RestoreWithNoStartupWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 2 more profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  Profile& profile1 =
      profiles::testing::CreateProfileSync(profile_manager, dest_path1);
  Profile& profile2 =
      profiles::testing::CreateProfileSync(profile_manager, dest_path2);
  DisableWhatsNewPage();

  // Set the profiles to open last visited pages.
  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(&profile1, pref_last);
  SessionStartupPref::SetStartupPref(&profile2, pref_last);

  Profile* default_profile = browser()->GetProfile();

  // TODO(crbug.com/40594327): Adapt this test for DestroyProfileOnBrowserClose
  // if needed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::SESSION_RESTORE,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive default_profile_keep_alive(
      default_profile, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile1_keep_alive(
      &profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      &profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open a page with profile1 and profile2.
  Browser* browser1 =
      CreateBrowserWindow(BrowserWindowCreateParams(
                              BrowserWindowInterface::TYPE_NORMAL, &profile1,
                              /*from_user_gesture=*/true))
          ->GetBrowserForMigrationOnly();
  chrome::NewTab(browser1, NewTabTypes::kNoUserAction);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/empty.html")));

  Browser* browser2 =
      CreateBrowserWindow(BrowserWindowCreateParams(
                              BrowserWindowInterface::TYPE_NORMAL, &profile2,
                              /*from_user_gesture=*/true))
          ->GetBrowserForMigrationOnly();
  chrome::NewTab(browser2, NewTabTypes::kNoUserAction);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/empty.html")));
  // Exit the browser, saving the multi-profile session state.
  chrome::ExecuteCommand(browser(), IDC_EXIT);
  ASSERT_TRUE(base::test::RunUntil(
      []() { return GlobalBrowserCollection::GetInstance()->IsEmpty(); }));

#if BUILDFLAG(IS_MAC)
  // While we closed all the browsers above, this doesn't quit the Mac app,
  // leaving the app in a half-closed state. Cancel the termination to put the
  // Mac app back into a known state.
  chrome_browser_application_mac::CancelTerminate();
#endif

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  dummy.AppendSwitch(switches::kNoStartupWindow);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles = {&profile1, &profile2};
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        {default_profile, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);

  // TODO(davidbienvenu): Waiting for some sort of browser is started
  // notification would be better. But, we're not opening any browser
  // windows, so we'd need to invent a new notification.
  content::RunAllTasksUntilIdle();

  // No browser windows should be opened.
  EXPECT_EQ(ProfileBrowserCollection::GetForProfile(&profile1)->GetSize(), 0u);
  EXPECT_EQ(ProfileBrowserCollection::GetForProfile(&profile2)->GetSize(), 0u);

  base::CommandLine empty(base::CommandLine::NO_PROGRAM);
  base::RunLoop run_loop;
  testing::SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 2);

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      empty, {}, {dest_path1, StartupProfileMode::kBrowserWindow});
  run_loop.Run();

  // profile1 and profile2 browser windows should be opened.
  EXPECT_EQ(ProfileBrowserCollection::GetForProfile(&profile1)->GetSize(), 1u);
  EXPECT_EQ(ProfileBrowserCollection::GetForProfile(&profile2)->GetSize(), 1u);
}

// Flaky. See https://crbug.com/41375379.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       DISABLED_ProfilesLaunchedAfterCrash) {
  // After an unclean exit, all profiles will be launched. However, they won't
  // open any pages automatically.

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 3 profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  base::FilePath dest_path3 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 3"));
  Profile& profile_home =
      profiles::testing::CreateProfileSync(profile_manager, dest_path1);
  Profile& profile_last =
      profiles::testing::CreateProfileSync(profile_manager, dest_path2);
  Profile& profile_urls =
      profiles::testing::CreateProfileSync(profile_manager, dest_path3);

  // Set the profiles to open the home page, last visited pages or URLs.
  SessionStartupPref pref_home(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(&profile_home, pref_home);

  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(&profile_last, pref_last);

  std::vector<GURL> urls;
  urls.push_back(chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls = urls;
  SessionStartupPref::SetStartupPref(&profile_urls, pref_urls);

  // Simulate a launch after an unclear exit.
  CloseBrowserAsynchronously(browser());
  ExitTypeService::GetInstanceForProfile(&profile_home)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  ExitTypeService::GetInstanceForProfile(&profile_last)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  ExitTypeService::GetInstanceForProfile(&profile_urls)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Use HistogramTester to make sure a bubble is shown when it's not on
  // platform Mac OS X and it's not official Chrome build.
  //
  // On Mac OS X, an infobar is shown to restore the previous session, which
  // is tested by function EnsureRestoreUIWasShown.
  //
  // Under a Google Chrome build, it is not tested because a task is posted to
  // the file thread before the bubble is shown. It is difficult to make sure
  // that the histogram check runs after all threads have finished their tasks.
  base::HistogramTester histogram_tester;
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  dummy.AppendSwitchASCII(switches::kTestType, "browser");
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(&profile_home);
  last_opened_profiles.push_back(&profile_last);
  last_opened_profiles.push_back(&profile_urls);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        {&profile_home, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);

  // No profiles are getting restored, since they all display the crash info
  // bar.
  EXPECT_FALSE(SessionRestore::IsRestoring(&profile_home));
  EXPECT_FALSE(SessionRestore::IsRestoring(&profile_last));
  EXPECT_FALSE(SessionRestore::IsRestoring(&profile_urls));

  // The profile which normally opens the home page displays the new tab page.
  BrowserWindowInterface* new_browser = nullptr;
  ASSERT_EQ(1u,
            ProfileBrowserCollection::GetForProfile(&profile_home)->GetSize());
  new_browser = FindOneOtherBrowserForProfile(&profile_home, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->GetTabStripModel();

  // The new browser should have only the NTP.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));

  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens last open pages displays the new tab page.
  ASSERT_EQ(1u,
            ProfileBrowserCollection::GetForProfile(&profile_last)->GetSize());
  new_browser = FindOneOtherBrowserForProfile(&profile_last, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->GetTabStripModel();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens URLs displays the new tab page.
  ASSERT_EQ(1u,
            ProfileBrowserCollection::GetForProfile(&profile_urls)->GetSize());
  new_browser = FindOneOtherBrowserForProfile(&profile_urls, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->GetTabStripModel();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Each profile should have one session restore bubble shown, so we should
  // observe count 3 in bucket 0 (which represents bubble shown).
  histogram_tester.ExpectBucketCount("Session.SessionCrashed.Bubble", 0, 3);
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       LaunchMultipleLockedProfiles) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager,
      user_data_dir.Append(FILE_PATH_LITERAL("New Profile 1")));
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager,
      user_data_dir.Append(FILE_PATH_LITERAL("New Profile 2")));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  std::vector<GURL> urls;
  urls.push_back(embedded_test_server()->GetURL("/title1.html"));
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(&profile1);
  last_opened_profiles.push_back(&profile2);
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(&profile2, pref);

  ProfileAttributesEntry* entry1 =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile1.GetPath());
  ASSERT_NE(entry1, nullptr);
  entry1->LockForceSigninProfile(true);

  ProfileAttributesEntry* entry2 =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile2.GetPath());
  ASSERT_NE(entry2, nullptr);
  entry2->LockForceSigninProfile(false);

  browser_creator.Start(command_line, profile_manager->user_data_dir(),
                        {&profile1, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);

  ASSERT_EQ(0u, ProfileBrowserCollection::GetForProfile(&profile1)->GetSize());
  ASSERT_EQ(1u, ProfileBrowserCollection::GetForProfile(&profile2)->GetSize());
}

#if BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       RemoteActivationTokenPropagation) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitchASCII("xdg-activation-token", "test-token-123");

  StartupProfilePathInfo path_info = {browser()->GetProfile()->GetPath(),
                                      StartupProfileMode::kBrowserWindow};

  ui_test_utils::BrowserCreatedObserver observer;

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(cmd_line, {},
                                                          path_info);

  Browser* new_browser = observer.Wait();
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(BrowserInitState::From(new_browser)->create_params().startup_id,
            "test-token-123");
}
#endif  // BUILDFLAG(IS_LINUX)

class StartupBrowserCreatorTestWithGuestParam
    : public StartupBrowserCreatorTest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsGuest() const { return GetParam(); }

  GURL GetTestURL() const { return GURL("https://www.youtube.com"); }

  // Creates a browser for a new profile (which may be Guest, based on
  // `IsGuest()`).
  Browser* CreateBrowser() {
    if (IsGuest()) {
      profiles::SwitchToGuestProfile();
    } else {
      base::FilePath profile_path = g_browser_process->profile_manager()
                                        ->GenerateNextProfileDirectoryPath();
      profiles::SwitchToProfile(profile_path, /*always_create=*/true);
    }
    Browser* test_browser = ui_test_utils::WaitForBrowserToOpen();
    profiles::SetLastUsedProfile(test_browser->GetProfile()->GetBaseName());
    return test_browser;
  }

  void OpenTabAlreadyRunning() {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendArg(GetTestURL().spec());
    ChromeBrowserMainParts::ProcessSingletonNotificationForTesting(
        command_line);
  }
};

// Tests that receiving a launch notification while Chrome is already running
// opens the URL in the current browser window.
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorTestWithGuestParam,
                       ProcessCommandLineAlreadyRunning) {
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BACKGROUND_MODE_MANAGER,
                             KeepAliveRestartOption::DISABLED);
  CloseBrowserSynchronously(browser());

  // Create a browser for a new profile.
  Browser* test_browser = CreateBrowser();
  ASSERT_TRUE(test_browser);
  ASSERT_EQ(test_browser->GetProfile()->IsGuestSession(), IsGuest());
  TabStripModel* tab_strip = test_browser->tab_strip_model();
  int initial_tab_count = tab_strip->count();

  // Open a URL while a browser is already open.
  ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
  OpenTabAlreadyRunning();
  content::WebContents* contents = tab_waiter.Wait();

  EXPECT_EQ(initial_tab_count + 1, tab_strip->count());
  EXPECT_EQ(contents, tab_strip->GetWebContentsAt(tab_strip->count() - 1));
  EXPECT_EQ(GetTestURL(), contents->GetVisibleURL());
}

// Tests that receiving a launch notification while Chrome is already running,
// but there was no browser window, reopens the last profile if it was regular,
// and opens the profile picker if it was guest.
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorTestWithGuestParam,
                       ProcessCommandLineAlreadyRunningAfterBrowserClose) {
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BACKGROUND_MODE_MANAGER,
                             KeepAliveRestartOption::DISABLED);
  CloseBrowserSynchronously(browser());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Create a browser for a new profile.
  Browser* test_browser = CreateBrowser();
  Profile* last_profile = test_browser->GetProfile();
  ASSERT_TRUE(test_browser);
  ASSERT_EQ(last_profile->IsGuestSession(), IsGuest());

  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive;
  if (!IsGuest()) {
    // Keep the profile alive to avoid unloading and immediately reloading it,
    // which causes some flakiness within the HistoryService.
    // This is not done for the guest profile because:
    // - the test scenario does not involve reloading the guest profile,
    // - it is not allowed to take a keep alive on a OTR profile.
    profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        last_profile, ProfileKeepAliveOrigin::kBackgroundMode);
  }

  CloseBrowserSynchronously(test_browser);
  // Closing the browser did not change the last used profile.
  EXPECT_EQ(profile_manager->GetLastUsedProfileDir(), last_profile->GetPath());
  ASSERT_FALSE(ProfilePicker::IsOpen());

  // Open a URL after the last active browser was closed.
  OpenTabAlreadyRunning();

  if (IsGuest()) {
    // The profile picker opens. There is no browser, the URL is not loaded.
    profiles::testing::WaitForPickerWidgetCreated();
    EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());
  } else {
    // The last used profile is reopened and the URL is loaded.
    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    Profile* profile = browser->GetProfile();
    EXPECT_FALSE(profile->IsGuestSession());
    TabStripModel* tab_strip = browser->tab_strip_model();
    EXPECT_EQ(
        tab_strip->GetWebContentsAt(tab_strip->count() - 1)->GetVisibleURL(),
        GetTestURL());
    EXPECT_FALSE(ProfilePicker::IsOpen());
    EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
    EXPECT_EQ(last_profile, profile);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         StartupBrowserCreatorTestWithGuestParam,
                         testing::Bool());

class StartupBrowserCreatorFirstRunTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorFirstRunTest() = default;
  StartupBrowserCreatorFirstRunTest(const StartupBrowserCreatorFirstRunTest&) =
      delete;
  StartupBrowserCreatorFirstRunTest& operator=(
      const StartupBrowserCreatorFirstRunTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
#if BUILDFLAG(IS_LINUX)
  bool SetUpUserDataDirectory() override;
#endif

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  policy::PolicyMap policy_map_;
};

void StartupBrowserCreatorFirstRunTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kForceFirstRun);
}

#if BUILDFLAG(IS_LINUX)
bool StartupBrowserCreatorFirstRunTest::SetUpUserDataDirectory() {
  if (!InProcessBrowserTest::SetUpUserDataDirectory()) {
    return false;
  }
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::WriteFile(user_data_dir.Append("EULA Accepted"), "");
  return true;
}
#endif

void StartupBrowserCreatorFirstRunTest::SetUpInProcessBrowserTestFixture() {
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Set a policy that prevents the first-run dialog from being shown.
  policy_map_.Set(
      policy::key::kMetricsReportingEnabled,
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
      policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  provider_.UpdateChromePolicy(policy_map_);
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                              /*is_first_policy_load_complete_return=*/true);
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest, AddFirstRunTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTabs(
      {embedded_test_server()->GetURL("/title1.html"),
       embedded_test_server()->GetURL("/title2.html")});

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IsFirstRun::kYes);
  launch.Launch(browser()->GetProfile(), chrome::startup::IsProcessStartup::kNo,
                /*restore_tabbed_browser=*/true);

  // This should have created a new browser window.
  BrowserWindowInterface* const new_browser =
      ui_test_utils::GetBrowserNotInSet({browser()});
  ASSERT_TRUE(new_browser);

  TabStripModel* const tab_strip = new_browser->GetTabStripModel();

  EXPECT_EQ(2, tab_strip->count());

  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetVisibleURL().ExtractFileName());
  EXPECT_EQ("title2.html",
            tab_strip->GetWebContentsAt(1)->GetVisibleURL().ExtractFileName());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_MAC)
// http://crbug.com/40339772
#define MAYBE_RestoreOnStartupURLsPolicySpecified \
  DISABLED_RestoreOnStartupURLsPolicySpecified
#else
#define MAYBE_RestoreOnStartupURLsPolicySpecified \
  RestoreOnStartupURLsPolicySpecified
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_RestoreOnStartupURLsPolicySpecified) {

  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;

  DisableWhatsNewPage();

  // Set the following user policies:
  // * RestoreOnStartup = RestoreOnStartupIsURLs
  // * RestoreOnStartupURLs = [ "/title1.html" ]
  policy_map_.Set(policy::key::kRestoreOnStartup,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD,
                  base::Value(SessionStartupPref::kPrefValueURLs), nullptr);
  base::ListValue startup_urls;
  startup_urls.Append(embedded_test_server()->GetURL("/title1.html").spec());
  policy_map_.Set(policy::key::kRestoreOnStartupURLs,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD,
                  base::Value(std::move(startup_urls)), nullptr);
  provider_.UpdateChromePolicy(policy_map_);
  base::RunLoop().RunUntilIdle();

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IsFirstRun::kYes);
  launch.Launch(browser()->GetProfile(),
                chrome::startup::IsProcessStartup::kYes,
                /*restore_tabbed_browser=*/true);

  // This should have created a new browser window.
  BrowserWindowInterface* const new_browser =
      ui_test_utils::GetBrowserNotInSet({browser()});
  ASSERT_TRUE(new_browser);

  // Verify that the URL specified through policy is shown and no sync promo has
  // been added.
  TabStripModel* const tab_strip = new_browser->GetTabStripModel();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetVisibleURL().ExtractFileName());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_MAC)
// http://crbug.com/40339772
#define MAYBE_FirstRunTabsWithRestoreSession \
  DISABLED_FirstRunTabsWithRestoreSession
#else
#define MAYBE_FirstRunTabsWithRestoreSession FirstRunTabsWithRestoreSession
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_FirstRunTabsWithRestoreSession) {
  // Simulate the following initial preferences:
  // {
  //  "first_run_tabs" : [
  //    "/title1.html"
  //  ],
  //  "session" : {
  //    "restore_on_startup" : 1
  //   },
  //   "sync_promo" : {
  //     "user_skipped" : true
  //   }
  // }
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTabs(
      {embedded_test_server()->GetURL("/title1.html")});
  browser()->GetProfile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IsFirstRun::kYes);
  launch.Launch(browser()->GetProfile(),
                chrome::startup::IsProcessStartup::kYes,
                /*restore_tabbed_browser=*/true);

  // This should have created a new browser window.
  BrowserWindowInterface* const new_browser =
      ui_test_utils::GetBrowserNotInSet({browser()});
  ASSERT_TRUE(new_browser);

  // Verify that the first-run tab is shown and no other pages are present.
  TabStripModel* const tab_strip = new_browser->GetTabStripModel();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetVisibleURL().ExtractFileName());
}

// Validates that prefs::kWasRestarted is automatically reset after next browser
// start.
class StartupBrowserCreatorWasRestartedFlag : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorWasRestartedFlag() = default;
  ~StartupBrowserCreatorWasRestartedFlag() override = default;

  // InProcessBrowserTest:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    static_cast<ChromeBrowserMainParts*>(browser_main_parts)
        ->AddParts(std::make_unique<BrowserCreatedMainParts>(base::BindOnce(
            &StartupBrowserCreatorWasRestartedFlag::OnFirstBrowserCreated,
            base::Unretained(this))));
  }
  bool SetUpUserDataDirectory() override {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    base::DictValue local_state;
    local_state.SetByDottedPath(prefs::kWasRestarted, true);
    std::string json = base::WriteJson(local_state).value_or("");

    base::FilePath local_state_path =
        user_data_dir.Append(chrome::kLocalStateFilename);
    if (!base::WriteFile(local_state_path, json)) {
      ADD_FAILURE() << "base::WriteFile() failed, " << local_state_path;
      return false;
    }

    return true;
  }

 protected:
  // SetUpCommandLine is setting kWasRestarted, so these tests all start up
  // with WasRestarted() true.
  void OnFirstBrowserCreated() {
    EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
    EXPECT_FALSE(
        g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted));
    on_browser_added_hit_ = true;
  }

  bool on_browser_added_hit_ = false;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorWasRestartedFlag, Test) {
  // OnBrowserAdded() should have been hit before the test body began.
  EXPECT_TRUE(on_browser_added_hit_);
  // This is a bit strange but what occurs is that StartupBrowserCreator runs
  // before this test body is hit and ~StartupBrowserCreator() will reset the
  // restarted state, so here when we read WasRestarted() it should already be
  // reset to false.
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(
      g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted));
}

enum class CommandLineFlagSecurityWarningsPolicy {
  kNoPolicy,
  kEnabled,
  kDisabled,
};

// Verifies that infobars are displayed (or not) depending on enterprise policy.
class StartupBrowserCreatorInfobarsTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<StartupBrowserCreatorFlagTypeValue,
                     CommandLineFlagSecurityWarningsPolicy>> {
 public:
  StartupBrowserCreatorInfobarsTest()
      : flag_type_(std::get<0>(GetParam())), policy_(std::get<1>(GetParam())) {}

 protected:
  std::pair<BrowserWindowInterface*, infobars::ContentInfoBarManager*>
  LaunchBrowserAndGetCreatedInfoBarManager(
      const base::CommandLine& command_line) {
    ui_test_utils::BrowserCreatedObserver browser_created_observer;

    EXPECT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
        command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
        {browser()->GetProfile(), StartupProfileMode::kBrowserWindow}, {}));

    // Wait until the new browser window has been created. Using
    // `FindOneOtherBrowser` is not sufficient here, because the window may be
    // created asynchronously.
    BrowserWindowInterface* new_browser = browser_created_observer.Wait();
    EXPECT_TRUE(new_browser);

    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(
            new_browser->GetTabStripModel()->GetWebContentsAt(0));
    EXPECT_TRUE(infobar_manager);

    return std::make_pair(new_browser, infobar_manager);
  }

  const StartupBrowserCreatorFlagTypeValue flag_type_;
  const CommandLineFlagSecurityWarningsPolicy policy_;

 private:
  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    if (policy_ != CommandLineFlagSecurityWarningsPolicy::kNoPolicy) {
      bool is_enabled =
          policy_ == CommandLineFlagSecurityWarningsPolicy::kEnabled;
      policy::PolicyMap policies;
      policies.Set(policy::key::kCommandLineFlagSecurityWarningsEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_PLATFORM, base::Value(is_enabled),
                   nullptr);
      policy_provider_.UpdateChromePolicy(policies);
    }
  }
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsTest, CheckInfobar) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  // We deliberately set the flag on the process command line instead of on the
  // command_line passed to the StartupBrowserCreator, because these flags are
  // all read from CommandLine::ForCurrentProcess and ignore the command line
  // passed to StartupBrowserCreator. In browser tests, this references the
  // browser test's instead of the new process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(flag_type_.flag);
  auto [browser, infobar_manager] =
      LaunchBrowserAndGetCreatedInfoBarManager(command_line);
  EXPECT_EQ(browser->GetType(), BrowserWindowInterface::Type::TYPE_NORMAL);

  EXPECT_EQ(HasInfoBar(infobar_manager, flag_type_.infobar_identifier),
            policy_ != CommandLineFlagSecurityWarningsPolicy::kDisabled);
}

// The trybots set the kNoSandbox flag when running browser tests with the
// address sanitizer enabled, which contradicts with the assumption of this test
// that there is no bad flag on the process command line.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_CheckInfobarOnlyUsesProcessCommandLine \
  DISABLED_CheckInfobarOnlyUsesProcessCommandLine
#else
#define MAYBE_CheckInfobarOnlyUsesProcessCommandLine \
  CheckInfobarOnlyUsesProcessCommandLine
#endif
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsTest,
                       MAYBE_CheckInfobarOnlyUsesProcessCommandLine) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  // The flag should not result in an infobar when not set on the process
  // command line via CommandLine::ForCurrentProcess.
  command_line.AppendSwitch(flag_type_.flag);
  auto [browser, infobar_manager] =
      LaunchBrowserAndGetCreatedInfoBarManager(command_line);
  EXPECT_EQ(browser->GetType(), BrowserWindowInterface::Type::TYPE_NORMAL);

  EXPECT_FALSE(HasInfoBar(infobar_manager, flag_type_.infobar_identifier));
}

INSTANTIATE_TEST_SUITE_P(
    PolicyControl,
    StartupBrowserCreatorInfobarsTest,
    ::testing::Combine(
        ::testing::Values(
            StartupBrowserCreatorFlagTypeValue{
                switches::kEnableAutomation,
                infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE},
            // Test one of the flags from |bad_flags_prompt.cc|. Any of the
            // flags should have the same behavior.
            StartupBrowserCreatorFlagTypeValue{
                switches::kDisableWebSecurity,
                infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE}),
        ::testing::Values(CommandLineFlagSecurityWarningsPolicy::kNoPolicy,
                          CommandLineFlagSecurityWarningsPolicy::kEnabled,
                          CommandLineFlagSecurityWarningsPolicy::kDisabled)),
    [](const testing::TestParamInfo<
        StartupBrowserCreatorInfobarsTest::ParamType>& info) {
      std::string policyState;
      switch (std::get<1>(info.param)) {
        case CommandLineFlagSecurityWarningsPolicy::kNoPolicy:
          policyState = "no policy";
          break;
        case CommandLineFlagSecurityWarningsPolicy::kEnabled:
          policyState = "policy enabled";
          break;
        case CommandLineFlagSecurityWarningsPolicy::kDisabled:
          policyState = "policy disabled";
          break;
      }

      std::string name = std::get<0>(info.param).flag + " " + policyState;
      std::replace_if(
          name.begin(), name.end(),
          [](unsigned char c) { return !absl::ascii_isalnum(c); }, '_');
      return name;
    });

// Verifies that infobars are displayed in the first browser window, even when
// the browser is started without an initial browser window by passing the
// `switches::kNoStartupWindow` command line switch.
class StartupBrowserCreatorInfobarsWithoutStartupWindowTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<StartupBrowserCreatorFlagTypeValue> {
 public:
  StartupBrowserCreatorInfobarsWithoutStartupWindowTest()
      : flag_type_(GetParam()) {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
    command_line->AppendSwitch(switches::kKeepAliveForTest);
  }

  std::pair<Browser*, infobars::ContentInfoBarManager*>
  LaunchBrowserAndGetCreatedInfoBarManager() {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();

    ui_test_utils::BrowserCreatedObserver browser_created_observer;
    StartupBrowserCreatorImpl launch(base::FilePath(), command_line,
                                     chrome::startup::IsFirstRun::kNo);
    launch.Launch(profile, chrome::startup::IsProcessStartup::kNo,
                  /*restore_tabbed_browser=*/true);
    Browser* new_browser = browser_created_observer.Wait();
    if (!new_browser) {
      return std::make_pair(nullptr, nullptr);
    }
    ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);

    return std::make_pair(
        new_browser, infobars::ContentInfoBarManager::FromWebContents(
                         new_browser->tab_strip_model()->GetWebContentsAt(0)));
  }

  const StartupBrowserCreatorFlagTypeValue flag_type_;
};

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsWithoutStartupWindowTest,
                       CheckInfobar) {
  // We deliberately set the flag on the process command line instead of on the
  // command_line passed to the StartupBrowserCreator, because these flags are
  // all read from `CommandLine::ForCurrentProcess` and ignore the command line
  // passed to `StartupBrowserCreator`. In browser tests, this references the
  // browser test's instead of the new process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(flag_type_.flag);

  EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());
  auto [browser, infobar_manager] = LaunchBrowserAndGetCreatedInfoBarManager();
  EXPECT_TRUE(browser);
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  ASSERT_TRUE(infobar_manager);
  EXPECT_TRUE(HasInfoBar(infobar_manager, flag_type_.infobar_identifier));

  // Now close and reopen the browser again - and re-check if the infobar is
  // there.
  CloseBrowserSynchronously(browser);

  EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());
  auto [browser2, infobar_manager2] =
      LaunchBrowserAndGetCreatedInfoBarManager();
  EXPECT_TRUE(browser2);
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  ASSERT_TRUE(infobar_manager2);
  EXPECT_EQ(flag_type_.is_global_infobar,
            HasInfoBar(infobar_manager2, flag_type_.infobar_identifier));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StartupBrowserCreatorInfobarsWithoutStartupWindowTest,
    ::testing::Values(
        StartupBrowserCreatorFlagTypeValue{
            switches::kEnableAutomation,
            infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE, true},
        // Test one of the flags from |bad_flags_prompt.cc|. Any of the
        // flags should have the same behavior.
        StartupBrowserCreatorFlagTypeValue{
            switches::kDisableWebSecurity,
            infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE, false}),
    [](const testing::TestParamInfo<
        StartupBrowserCreatorInfobarsWithoutStartupWindowTest::ParamType>&
           info) {
      std::string name = info.param.flag;
      std::replace_if(
          name.begin(), name.end(),
          [](unsigned char c) { return !absl::ascii_isalnum(c); }, '_');
      return name;
    });

// Verifies that infobars are not displayed in Kiosk mode.
class StartupBrowserCreatorInfobarsKioskTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorInfobarsKioskTest() = default;

 protected:
  infobars::ContentInfoBarManager*
  LaunchKioskBrowserAndGetCreatedInfoBarManager(
      const std::string& extra_switch) {
    Profile* profile = browser()->GetProfile();

    // CommandLine::ForCurrentProcess is used to determine whether kiosk mode is
    // enabled instead of the command-line passed to StartupBrowserCreator. In
    // browser tests, this references the browser test's instead of the new
    // process.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kKioskMode);

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(extra_switch);
    StartupBrowserCreatorImpl launch(base::FilePath(), command_line,
                                     chrome::startup::IsFirstRun::kNo);
    launch.Launch(profile, chrome::startup::IsProcessStartup::kYes,
                  /*restore_tabbed_browser=*/true);

    // This should have created a new browser window.
    BrowserWindowInterface* const new_browser =
        ui_test_utils::GetBrowserNotInSet({browser()});
    EXPECT_TRUE(new_browser);
    if (!new_browser) {
      return nullptr;
    }

    return infobars::ContentInfoBarManager::FromWebContents(
        new_browser->GetTabStripModel()->GetActiveWebContents());
  }
};

// Verify that the Automation Enabled infobar is still shown in Kiosk mode.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorInfobarsKioskTest,
                       CheckInfobarForEnableAutomation) {
  // CommandLine::ForCurrentProcess is used to determine whether automation is
  // enabled instead of the command-line passed to StartupBrowserCreator. In
  // browser tests, this references the browser test's instead of the new
  // process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAutomation);

  // Passing the kEnableAutomation argument here presently does not do
  // anything because of the aforementioned limitation.
  infobars::ContentInfoBarManager* infobar_manager =
      LaunchKioskBrowserAndGetCreatedInfoBarManager(
          switches::kEnableAutomation);
  ASSERT_TRUE(infobar_manager);

  EXPECT_TRUE(HasInfoBar(
      infobar_manager, infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE));
}

// Verify that the Bad Flags infobar is not shown in kiosk mode.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorInfobarsKioskTest,
                       CheckInfobarForBadFlag) {
  // BadFlagsPrompt::ShowBadFlagsPrompt uses CommandLine::ForCurrentProcess
  // instead of the command-line passed to StartupBrowserCreator. In browser
  // tests, this references the browser test's instead of the new process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebSecurity);

  // Passing the kDisableWebSecurity argument here presently does not do
  // anything because of the aforementioned limitation.
  // https://crbug.com/40121975
  infobars::ContentInfoBarManager* infobar_manager =
      LaunchKioskBrowserAndGetCreatedInfoBarManager(
          switches::kDisableWebSecurity);
  ASSERT_TRUE(infobar_manager);

  EXPECT_FALSE(HasInfoBar(
      infobar_manager, infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE));
}

// Checks the correct behavior of the profile picker on startup.
class StartupBrowserCreatorPickerTestBase : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorPickerTestBase() {
    // This test configures command line params carefully. Make sure
    // InProcessBrowserTest does _not_ add about:blank as a startup URL to the
    // command line.
    set_open_about_blank_on_browser_launch(false);
  }
  StartupBrowserCreatorPickerTestBase(
      const StartupBrowserCreatorPickerTestBase&) = delete;
  StartupBrowserCreatorPickerTestBase& operator=(
      const StartupBrowserCreatorPickerTestBase&) = delete;
  ~StartupBrowserCreatorPickerTestBase() override = default;

  void CreateMultipleProfiles() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    // Create two additional profiles because the main test profile is created
    // later in the startup process and so we need to have at least 2 fake
    // profiles.
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::vector<base::FilePath> profile_paths = {
        profile_manager->user_data_dir().Append(
            FILE_PATH_LITERAL("New Profile 1")),
        profile_manager->user_data_dir().Append(
            FILE_PATH_LITERAL("New Profile 2"))};
    for (int i = 0; i < 2; ++i) {
      const base::FilePath& profile_path = profile_paths[i];
      profiles::testing::CreateProfileSync(profile_manager, profile_path);
      // Mark newly created profiles as active.
      ProfileAttributesEntry* entry =
          profile_manager->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(profile_path);
      ASSERT_NE(entry, nullptr);
      entry->SetActiveTimeToNow();
      entry->SetAuthInfo(
          GaiaId(base::StringPrintf("gaia_id_%i", i)),
          base::UTF8ToUTF16(base::StringPrintf("user%i@gmail.com", i)),
          /*is_consented_primary_account=*/false);
    }
  }
};

struct ProfilePickerSetup {
  enum class ShutdownType {
    kNormal,  // Normal shutdown (e.g. by closing the browser window).
    kExit,    // Exit through the application menu.
    kRestart  // Restart (e.g. after an update).
  };

  bool expected_to_show;
  std::optional<std::string> switch_name;
  std::optional<std::string> switch_value_ascii;
  std::optional<GURL> url_arg;
  ShutdownType shutdown_type = ShutdownType::kNormal;
  std::optional<std::string> extra_switch_name;
};

// Checks the correct behavior of the profile picker on startup. This feature is
// not available on ChromeOS.
class StartupBrowserCreatorPickerTest
    : public StartupBrowserCreatorPickerTestBase,
      public ::testing::WithParamInterface<ProfilePickerSetup> {
 public:
  StartupBrowserCreatorPickerTest()
      : relaunch_chrome_override_(base::BindRepeating(
            [](const base::CommandLine&) { return true; })) {}
  StartupBrowserCreatorPickerTest(const StartupBrowserCreatorPickerTest&) =
      delete;
  StartupBrowserCreatorPickerTest& operator=(
      const StartupBrowserCreatorPickerTest&) = delete;
  ~StartupBrowserCreatorPickerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    StartupBrowserCreatorPickerTestBase::SetUpCommandLine(command_line);

    if (content::IsPreTest()) {
      return;  // Don't apply the test parameters to the PRE test.
    }

    if (GetParam().url_arg) {
      command_line->AppendArg(GetParam().url_arg->spec());
    }
    if (GetParam().switch_value_ascii) {
      DCHECK(GetParam().switch_name);
      command_line->AppendSwitchASCII(*GetParam().switch_name,
                                      *GetParam().switch_value_ascii);
    } else if (GetParam().switch_name) {
      command_line->AppendSwitch(*GetParam().switch_name);
    }
    if (GetParam().extra_switch_name) {
      command_line->AppendSwitch(*GetParam().extra_switch_name);
    }
  }

 private:
  // Prevent the browser from automatically relaunching in the PRE_ test. The
  // browser will be relaunched by the main test.
  upgrade_util::ScopedRelaunchChromeBrowserOverride relaunch_chrome_override_;
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerTest, PRE_TestSetup) {
  CreateMultipleProfiles();

  switch (GetParam().shutdown_type) {
    case ProfilePickerSetup::ShutdownType::kNormal:
      // Need to close the browser window manually so that the real test does
      // not treat it as session restore.
      CloseAllBrowsers();
      break;
    case ProfilePickerSetup::ShutdownType::kExit:
      chrome::AttemptExit();
      break;
    case ProfilePickerSetup::ShutdownType::kRestart:
      chrome::AttemptRestart();
      break;
  }

  ASSERT_EQ(
      g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted),
      GetParam().shutdown_type == ProfilePickerSetup::ShutdownType::kRestart);
}

// Checks that either the ProfilePicker or a browser window is open at startup.
// Except with switches::kNoStartupWindow, for which neither the picker nor a
// browser is open.
// TODO(crbug.com/394713545): Flaky on all of Win/Mac/Linux
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerTest, DISABLED_TestSetup) {
  ProfilePickerSetup setup_param = GetParam();

  // Check the ProfilePicker.
  if (setup_param.expected_to_show) {
    if (!ProfilePicker::IsOpen()) {
      base::RunLoop run_loop;
      ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
          run_loop.QuitClosure());
      run_loop.Run();
    }
    EXPECT_TRUE(ProfilePicker::IsOpen());
  } else {
    EXPECT_FALSE(ProfilePicker::IsOpen());
  }

  // Check the browser window.
  if (setup_param.expected_to_show ||
      setup_param.switch_name == switches::kNoStartupWindow) {
    EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());
  } else {
    EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  }

  // No Guest profile was created.
  for (const Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    EXPECT_FALSE(profile->IsGuestSession());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StartupBrowserCreatorPickerTest,
    ::testing::Values(
// Flaky: https://crbug.com/40148327
#if !BUILDFLAG(IS_OZONE)
        // Picker should be shown in normal multi-profile startup situation.
        ProfilePickerSetup{/*expected_to_show=*/true},
#endif
        // Skip the picker for various command-line params and use the last used
        // profile, instead.
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kIncognito},
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kNoStartupWindow},
        // Skip the picker when a specific profile is requested (used e.g. by
        // profile specific desktop shortcuts on Win).
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kProfileDirectory,
                           /*switch_value_ascii=*/"Default"},
        // Same, but with the kIgnoreProfileDirectoryIfNotExists flag with the
        // profile existing.
        ProfilePickerSetup{
            /*expected_to_show=*/false,
            /*switch_name=*/switches::kProfileDirectory,
            /*switch_value_ascii=*/"Default",
            /*url_arg=*/std::nullopt,
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kNormal,
            /*extra_switch_name=*/
            switches::kIgnoreProfileDirectoryIfNotExists},
        // Show the picker if the profile is ignored due to it not existing.
        ProfilePickerSetup{
            /*expected_to_show=*/true,
            /*switch_name=*/switches::kProfileDirectory,
            /*switch_value_ascii=*/"DoesNotExist",
            /*url_arg=*/std::nullopt,
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kNormal,
            /*extra_switch_name=*/switches::kIgnoreProfileDirectoryIfNotExists},
        // Skip the picker when a URL is provided on command-line (used by the
        // OS when Chrome is the default web browser) and use the last used
        // profile, instead.
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/std::nullopt,
                           /*switch_value_ascii=*/std::nullopt,
                           /*url_arg=*/GURL("https://www.foo.com/")},
        // Regression test for http://crbug.com/40741954
        // Picker should be shown after exit.
        ProfilePickerSetup{
            /*expected_to_show=*/true,
            /*switch_name=*/std::nullopt,
            /*switch_value_ascii=*/std::nullopt,
            /*url_arg=*/std::nullopt,
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kExit},
        // Regression test for http://crbug.com/40196098
        // Picker should not be shown after restart.
        ProfilePickerSetup{
            /*expected_to_show=*/false,
            /*switch_name=*/std::nullopt,
            /*switch_value_ascii=*/std::nullopt,
            /*url_arg=*/std::nullopt,
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kRestart},
        // Skip the picker when a url is requested and the profile is ignored.
        ProfilePickerSetup{
            /*expected_to_show=*/false,
            /*switch_name=*/switches::kProfileDirectory,
            /*switch_value_ascii=*/"DoesNotExist",
            /*url_arg=*/GURL("https://www.foo.com/"),
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kNormal,
            /*extra_switch_name=*/
            switches::kIgnoreProfileDirectoryIfNotExists}));

// Skips the profile picker when the specified profile email matches one of the
// active profiles.
class StartupBrowserCreatorPickerProfileEmailTest
    : public StartupBrowserCreatorPickerTestBase {
 public:
  StartupBrowserCreatorPickerProfileEmailTest() = default;
  StartupBrowserCreatorPickerProfileEmailTest(
      const StartupBrowserCreatorPickerProfileEmailTest&) = delete;
  StartupBrowserCreatorPickerProfileEmailTest& operator=(
      const StartupBrowserCreatorPickerProfileEmailTest&) = delete;
  ~StartupBrowserCreatorPickerProfileEmailTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    StartupBrowserCreatorPickerTestBase::SetUpCommandLine(command_line);
    if (content::IsPreTest()) {
      return;
    }
    command_line->AppendSwitchASCII(switches::kProfileEmail, "user0@gmail.com");
  }

 private:
  // Prevent the browser from automatically relaunching in the PRE_ test. The
  // browser will be relaunched by the main test.
  upgrade_util::ScopedRelaunchChromeBrowserOverride relaunch_chrome_override_{
      base::BindRepeating([](const base::CommandLine&) { return true; })};
};

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorPickerProfileEmailTest,
                       PRE_TestSetup) {
  CreateMultipleProfiles();

  // Need to close the browser window manually so that the main test does
  // not treat it as session restore.
  CloseAllBrowsers();

  // Ensure everything is written to disk before moving on to the main test.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorPickerProfileEmailTest, TestSetup) {
  // Check the ProfilePicker.
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // Check the browser window.
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());

  // No Guest profile was created.
  for (const Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    EXPECT_FALSE(profile->IsGuestSession());
  }
}

// Shows the profile picker when there are multiple active profiles and the
// specified profile email doesn't match any of them.
class StartupBrowserCreatorPickerUnknownProfileEmail
    : public StartupBrowserCreatorPickerTestBase {
 public:
  StartupBrowserCreatorPickerUnknownProfileEmail() = default;
  StartupBrowserCreatorPickerUnknownProfileEmail(
      const StartupBrowserCreatorPickerUnknownProfileEmail&) = delete;
  StartupBrowserCreatorPickerUnknownProfileEmail& operator=(
      const StartupBrowserCreatorPickerUnknownProfileEmail&) = delete;
  ~StartupBrowserCreatorPickerUnknownProfileEmail() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    StartupBrowserCreatorPickerTestBase::SetUpCommandLine(command_line);
    if (content::IsPreTest()) {
      return;
    }
    command_line->AppendSwitchASCII(switches::kProfileEmail,
                                    "unknown@gmail.com");
  }

 private:
  // Prevent the browser from automatically relaunching in the PRE_ test. The
  // browser will be relaunched by the main test.
  upgrade_util::ScopedRelaunchChromeBrowserOverride relaunch_chrome_override_{
      base::BindRepeating([](const base::CommandLine&) { return true; })};
};

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorPickerUnknownProfileEmail,
                       PRE_TestSetup) {
  CreateMultipleProfiles();

  // Need to close the browser window manually so that the main test does
  // not treat it as session restore.
  CloseAllBrowsers();

  // Ensure everything is written to disk before moving on to the main test.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorPickerUnknownProfileEmail,
                       TestSetup) {
  // Check the ProfilePicker.
  if (!ProfilePicker::IsOpen()) {
    base::RunLoop run_loop;
    ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // Check the browser window.
  EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());

  // No Guest profile was created.
  for (const Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    EXPECT_FALSE(profile->IsGuestSession());
  }
}

// Shows the profile picker for an unknown email with
// kCreateProfileEmailIfNotExists switch, even with a URL on the command line.
class StartupBrowserCreatorPickerUnknownEmailCreateProfileIfNotExists
    : public StartupBrowserCreatorPickerTestBase {
 public:
  StartupBrowserCreatorPickerUnknownEmailCreateProfileIfNotExists() = default;
  StartupBrowserCreatorPickerUnknownEmailCreateProfileIfNotExists(
      const StartupBrowserCreatorPickerUnknownEmailCreateProfileIfNotExists&) =
      delete;
  StartupBrowserCreatorPickerUnknownEmailCreateProfileIfNotExists& operator=(
      const StartupBrowserCreatorPickerUnknownEmailCreateProfileIfNotExists&) =
      delete;
  ~StartupBrowserCreatorPickerUnknownEmailCreateProfileIfNotExists() override =
      default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    StartupBrowserCreatorPickerTestBase::SetUpCommandLine(command_line);
    if (content::IsPreTest()) {
      return;
    }
    command_line->AppendArg("https://www.foo.com/");
    command_line->AppendSwitchASCII(switches::kProfileEmail,
                                    "unknown@gmail.com");
    command_line->AppendSwitch(switches::kCreateProfileEmailIfNotExists);
  }

 private:
  // Prevent the browser from automatically relaunching in the PRE_ test. The
  // browser will be relaunched by the main test.
  upgrade_util::ScopedRelaunchChromeBrowserOverride relaunch_chrome_override_{
      base::BindRepeating([](const base::CommandLine&) { return true; })};
};

IN_PROC_BROWSER_TEST_F(
    StartupBrowserCreatorPickerUnknownEmailCreateProfileIfNotExists,
    PRE_TestSetup) {
  CreateMultipleProfiles();

  // Need to close the browser window manually so that the main test does
  // not treat it as session restore.
  CloseAllBrowsers();

  // Ensure everything is written to disk before moving on to the main test.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserCreatorPickerUnknownEmailCreateProfileIfNotExists,
    TestSetup) {
  // Check the ProfilePicker.
  if (!ProfilePicker::IsOpen()) {
    base::RunLoop run_loop;
    ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // Check the browser window.
  EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());

  // No Guest profile was created.
  for (const Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    EXPECT_FALSE(profile->IsGuestSession());
  }
}

class GuestStartupBrowserCreatorPickerTest
    : public StartupBrowserCreatorPickerTestBase {
 public:
  GuestStartupBrowserCreatorPickerTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGuest);
  }
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_F(GuestStartupBrowserCreatorPickerTest,
                       PRE_SkipsPickerWithGuest) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_F(GuestStartupBrowserCreatorPickerTest,
                       SkipsPickerWithGuest) {
  // The picker is skipped which means a browser window is opened on startup.
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  EXPECT_TRUE(browser()->GetProfile()->IsGuestSession());
}

class StartupBrowserCreatorPickerNoParamsTest
    : public StartupBrowserCreatorPickerTestBase {};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorPickerNoParamsTest,
                       PRE_ShowPickerWhenAlreadyLaunched) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorPickerNoParamsTest,
                       ShowPickerWhenAlreadyLaunched) {
  // Preprequisite: The picker is shown on the first start-up
  profiles::testing::WaitForPickerWidgetCreated();
  ASSERT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());

  // Close the picker.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ProfilePicker::Hide();
  profiles::testing::WaitForPickerClosed();
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // Simulate a second start when the browser is already running.
  base::FilePath current_dir = base::FilePath();
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupProfilePathInfo startup_profile_path_info =
      GetStartupProfilePath(current_dir, command_line,
                            /*ignore_profile_picker=*/false);
  EXPECT_EQ(startup_profile_path_info.mode, StartupProfileMode::kProfilePicker);
  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, current_dir, startup_profile_path_info);

  // The picker is shown again if no profile was previously opened.
  profiles::testing::WaitForPickerWidgetCreated();
  EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());
}

class SearchQueryStartupBrowserCreatorPickerTest
    : public StartupBrowserCreatorPickerTestBase {
 public:
  SearchQueryStartupBrowserCreatorPickerTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendArg("? Foo");
  }
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_F(SearchQueryStartupBrowserCreatorPickerTest,
                       PRE_SkipsPickerWithCommandLineSearchQuery) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_F(SearchQueryStartupBrowserCreatorPickerTest,
                       SkipsPickerWithCommandLineSearchQuery) {
  // A browser window is shown on start-up because the command line contains a
  // search query.
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());

  // Check the return value of `GetStartupProfilePath()` explicitly.
  base::FilePath current_dir = base::FilePath();
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendArg("? Foo");
  StartupProfilePathInfo startup_profile_path_info =
      GetStartupProfilePath(current_dir, command_line,
                            /*ignore_profile_picker=*/false);
  EXPECT_EQ(startup_profile_path_info.mode, StartupProfileMode::kBrowserWindow);
}

class StartupBrowserCreatorPickerInfobarTest
    : public StartupBrowserCreatorPickerTestBase,
      public ::testing::WithParamInterface<StartupBrowserCreatorFlagTypeValue> {
 public:
  StartupBrowserCreatorPickerInfobarTest() : flag_type_(GetParam()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {}

 protected:
  // Simulates a click on a profile card. The profile picker must be already
  // opened.
  void OpenProfileFromPicker(const base::FilePath& profile_path,
                             bool open_settings) {
    base::ListValue args;
    args.Append(base::FilePathToValue(profile_path));
    profile_picker_handler()->HandleLaunchSelectedProfile(open_settings, args);
  }

  // Returns the profile picker webUI handler. The profile picker must be opened
  // before calling this function.
  ProfilePickerHandler* profile_picker_handler() {
    DCHECK(ProfilePicker::IsOpen());

    views::WebView* web_view = ProfilePicker::GetWebViewForTesting();
    if (web_view == nullptr) {
      return nullptr;
    }

    return web_view->GetWebContents()
        ->GetWebUI()
        ->GetController()
        ->GetAs<ProfilePickerUI>()
        ->GetProfilePickerHandlerForTesting();
  }

  const StartupBrowserCreatorFlagTypeValue flag_type_;
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerInfobarTest,
                       PRE_ShowsEnableAutomationInfobar) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerInfobarTest,
                       ShowsEnableAutomationInfobar) {
  EXPECT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());

  // We deliberately set the flag on the process command line instead of on the
  // command_line passed to the StartupBrowserCreator, because these flags are
  // always read from the command line of the current process
  base::CommandLine::ForCurrentProcess()->AppendSwitch(flag_type_.flag);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile = profile_manager->GetLastUsedProfile();
  }

  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  OpenProfileFromPicker(profile->GetPath(), false);
  Browser* new_browser = browser_created_observer.Wait();
  ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(
          new_browser->tab_strip_model()->GetWebContentsAt(0));

  EXPECT_TRUE(HasInfoBar(infobar_manager, flag_type_.infobar_identifier));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StartupBrowserCreatorPickerInfobarTest,
    ::testing::Values(
        StartupBrowserCreatorFlagTypeValue{
            switches::kEnableAutomation,
            infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE},
        // Test one of the flags from |bad_flags_prompt.cc|. Any of the
        // flags should have the same behavior.
        StartupBrowserCreatorFlagTypeValue{
            switches::kDisableWebSecurity,
            infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE}),
    [](const testing::TestParamInfo<
        StartupBrowserCreatorPickerInfobarTest::ParamType>& info) {
      std::string name = info.param.flag;
      std::replace_if(
          name.begin(), name.end(),
          [](unsigned char c) { return !absl::ascii_isalnum(c); }, '_');
      return name;
    });

class StartupBrowserCreatorOpenUrlsInNextProfileCreatedTest
    : public StartupBrowserCreatorPickerTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    StartupBrowserCreatorPickerTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kProfileEmail, "test@gmail.com");
    command_line->AppendSwitch(switches::kCreateProfileEmailIfNotExists);
    command_line->AppendArg("https://www.google.com");
  }
};

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorOpenUrlsInNextProfileCreatedTest,
                       OpenUrlsInNextProfileCreated) {
  ASSERT_TRUE(ProfilePicker::GetOpenCommandLineUrlsInNextProfileOpened());
  ASSERT_TRUE(ProfilePicker::IsOpen());
  ASSERT_EQ(0u, GlobalBrowserCollection::GetInstance()->GetSize());
}

class StartupBrowserCreatorTestRootStoreFlagTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorTestRootStoreFlagTest() {
    scoped_feature_list_.InitAndEnableFeature(net::features::kTestRootStore);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
    command_line->AppendSwitch(switches::kKeepAliveForTest);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTestRootStoreFlagTest,
                       CheckInfobarIsShown) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();

  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line,
                                   chrome::startup::IsFirstRun::kNo);
  launch.Launch(profile, chrome::startup::IsProcessStartup::kNo,
                /*restore_tabbed_browser=*/true);
  Browser* new_browser = browser_created_observer.Wait();
  ASSERT_TRUE(new_browser);
  ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);

  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(
          new_browser->tab_strip_model()->GetWebContentsAt(0));
  ASSERT_TRUE(infobar_manager);
  EXPECT_TRUE(HasInfoBar(
      infobar_manager, infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE));
}
