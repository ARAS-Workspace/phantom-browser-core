// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/settings/on_device_ai_settings_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/browsing_data/core/features.h"
#include "components/compose/buildflags.h"
#include "components/content_settings/core/common/features.h"
#include "components/history/core/browser/features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/permissions/features.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "crypto/crypto_buildflags.h"
#include "device/fido/public/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/compositor/compositor_switches.h"

#include "chrome/browser/ui/toasts/toast_features.h"  // nogncheck



class SettingsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  SettingsBrowserTest() {
    set_test_loader_host(chrome::kChromeUISettingsHost);
  }
};

using SettingsTest = SettingsBrowserTest;

// Note: Keep tests below in alphabetical ordering.

IN_PROC_BROWSER_TEST_F(SettingsTest, AccountPage) {
  RunTest("settings/account_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AntiAbusePage) {
  RunTest("settings/anti_abuse_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AppearanceFontsPage) {
  RunTest("settings/appearance_fonts_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AppearancePageIndex) {
  RunTest("settings/appearance_page_index_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AppearancePage) {
  RunTest("settings/appearance_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, BatteryPage) {
  RunTest("settings/battery_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, CategorySettingExceptions) {
  RunTest("settings/category_setting_exceptions_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Checkbox) {
  RunTest("settings/checkbox_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ChooserExceptionList) {
  RunTest("settings/chooser_exception_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ChooserExceptionListEntry) {
  RunTest("settings/chooser_exception_list_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, CollapseRadioButton) {
  RunTest("settings/collapse_radio_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ControlledButton) {
  RunTest("settings/controlled_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ControlledRadioButton) {
  RunTest("settings/controlled_radio_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, CrPolicyPrefIndicator) {
  RunTest("settings/cr_policy_pref_indicator_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, DefaultBrowser) {
  RunTest("settings/default_browser_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, DoNotTrackToggle) {
  RunTest("settings/do_not_track_toggle_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, DownloadsPage) {
  RunTest("settings/downloads_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, DropdownMenu) {
  RunTest("settings/dropdown_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ExtensionControlledIndicator) {
  RunTest("settings/extension_controlled_indicator_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ExtensionControlledMessage) {
  RunTest("settings/extension_controlled_message_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, FileSystemSettingsSiteDetails) {
  RunTest("settings/file_system_site_details_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, FileSystemSettingsList) {
  RunTest("settings/file_system_site_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, FileSystemSettingsListEntries) {
  RunTest("settings/file_system_site_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, FileSystemSettingsListEntryItems) {
  RunTest("settings/file_system_site_entry_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, HelpPage) {
  RunTest("settings/help_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, HomeUrlInput) {
  RunTest("settings/home_url_input_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ImportDataDialog) {
  RunTest("settings/import_data_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Languages) {
  RunTest("settings/languages_test.js", "mocha.run()");
}

// Copied from Polymer 2 version of tests:
// Times out on Windows Tests (dbg). See https://crbug.com/41278078.
// Times out / crashes on chromium.linux/Linux Tests (dbg) crbug.com/41287641
// Flaky everywhere crbug.com/40760356
IN_PROC_BROWSER_TEST_F(SettingsTest, DISABLED_MainPage) {
  RunTest("settings/settings_main_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_LINUX)
#define MAYBE_SettingsMain DISABLED_SettingsMain
#else
#define MAYBE_SettingsMain SettingsMain
#endif
IN_PROC_BROWSER_TEST_F(SettingsTest, MAYBE_SettingsMain) {
  RunTest("settings/settings_main_plugins_test.js", "mocha.run()");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(SettingsTest, MetricsReporting) {
  RunTest("settings/metrics_reporting_test.js",
          "runMochaSuite('MetricsReporting')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, MetricsConsentRestructureDisabled) {
  RunTest("settings/metrics_reporting_test.js",
          "runMochaSuite('MetricsConsentRestructureDisabled')");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, PerformancePageIndex) {
  RunTest("settings/performance_page_index_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PeoplePage) {
  RunTest("settings/people_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PeoplePageIndex) {
  RunTest("settings/people_page_index_test.js", "mocha.run()");
}


IN_PROC_BROWSER_TEST_F(SettingsTest, PeoplePageManageProfile) {
  RunTest("settings/people_page_manage_profile_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PeoplePageSyncControls) {
  RunTest("settings/people_page_sync_controls_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PrivacyPageIndex) {
  RunTest("settings/privacy_page_index_test.js",
          "runMochaSuite('PrivacyPageIndex Main')");
}

// TODO(crbug.com/533057215): Flaky on Linux debug builds.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PrivacyPageIndexSiteSettings DISABLED_PrivacyPageIndexSiteSettings
#else
#define MAYBE_PrivacyPageIndexSiteSettings PrivacyPageIndexSiteSettings
#endif
IN_PROC_BROWSER_TEST_F(SettingsTest, MAYBE_PrivacyPageIndexSiteSettings) {
  RunTest("settings/privacy_page_index_test.js",
          "runMochaSuite('PrivacyPageIndex SiteSettings')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Prefs) {
  RunTest("settings/settings_prefs_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PrefService) {
  RunTest("settings/pref_service_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PrefServiceObserverMixin) {
  RunTest("settings/pref_service_observer_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PrefServiceObserverMixinLit) {
  RunTest("settings/pref_service_observer_mixin_lit_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PrefUtils) {
  RunTest("settings/settings_pref_util_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ProtectedContentPage) {
  RunTest("settings/protected_content_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SecurityPageFeatureRow) {
  RunTest("settings/security/security_page_feature_row_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, WebuiRefresh2026) {
  RunTest("settings/settings_ui_test.js", "runMochaSuite('WebuiRefresh2026')");
}

// Timeout on Linux dbg bots: https://crbug.com/40881745
#if !(BUILDFLAG(IS_LINUX) && !defined(NDEBUG))
IN_PROC_BROWSER_TEST_F(SettingsTest, SyncSettings) {
  RunTest("settings/people_page_sync_page_test.js",
          "runMochaSuite('SyncSettings')");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest,
                       SyncSettingsWithReplaceSyncPromosWithSignInPromos) {
  RunTest("settings/people_page_sync_page_test.js",
          "runMochaSuite('SyncSettingsWithReplaceSyncPromosWithSignInPromos')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, EEAChoiceCountry) {
  RunTest("settings/people_page_sync_page_test.js",
          "runMochaSuite('EEAChoiceCountry')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, FeatureShortcutsPage) {
  RunTest("settings/feature_shortcuts_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, KeyboardShortcutPage) {
  RunTest("settings/keyboard_shortcut_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ProtocolHandlers) {
  RunTest("settings/protocol_handlers_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, RecentSitePermissions) {
  RunTest("settings/recent_site_permissions_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, RelaunchConfirmationDialog) {
  RunTest("settings/relaunch_confirmation_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ResetPage) {
  RunTest("settings/reset_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ResetProfileBanner) {
  RunTest("settings/reset_profile_banner_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ScrollableMixin) {
  RunTest("settings/scrollable_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Search) {
  RunTest("settings/search_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SearchableViewContainerMixin) {
  RunTest("settings/searchable_view_container_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SearchableViewContainerMixinLit) {
  RunTest("settings/searchable_view_container_mixin_lit_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SearchEngineEditDialog) {
  RunTest("settings/search_engine_edit_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SearchEngineEntry) {
  RunTest("settings/search_engine_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SearchEngineIcon) {
  RunTest("settings/search_engine_icon_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SearchEngineListDialog) {
  RunTest("settings/search_engine_list_dialog_test.js", "mocha.run()");
}

// TODO(crbug.com/448517054): Flaky on Linux debug builds.
#if (BUILDFLAG(IS_LINUX) && !defined(NDEBUG))
#define MAYBE_SearchEngines DISABLED_SearchEngines
#else
#define MAYBE_SearchEngines SearchEngines
#endif
IN_PROC_BROWSER_TEST_F(SettingsTest, MAYBE_SearchEngines) {
  RunTest("settings/search_engines_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SearchPageIndex) {
  RunTest("settings/search_page_index_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SearchPage) {
  RunTest("settings/search_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Section) {
  RunTest("settings/settings_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SecureDnsInput) {
  RunTest("settings/security/secure_dns_test.js",
          "runMochaSuite('SettingsSecureDnsInput')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SecureDns) {
  RunTest("settings/security/secure_dns_test.js",
          "runMochaSuite('SettingsSecureDns')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SecureDnsV2Input) {
  RunTest("settings/security/secure_dns_v2_test.js",
          "runMochaSuite('SettingsSecureDnsV2Input')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SecureDnsV2) {
  RunTest("settings/security/secure_dns_v2_test.js",
          "runMochaSuite('SettingsSecureDnsV2')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SettingsCategoryDefaultRadioGroup) {
  RunTest("settings/settings_category_default_radio_group_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SettingsMenu) {
  RunTest("settings/settings_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SettingsRadioGroup) {
  RunTest("settings/settings_radio_group_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SimpleConfirmationDialog) {
  RunTest("settings/simple_confirmation_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteDataTest) {
  RunTest("settings/site_data_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteDetailsPermission) {
  RunTest("settings/site_details_permission_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteDetailsPermissionDeviceEntry) {
  RunTest("settings/site_details_permission_device_entry_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteEntry) {
  RunTest("settings/site_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteFavicon) {
  RunTest("settings/site_favicon_test.js", "mocha.run()");
}


IN_PROC_BROWSER_TEST_F(SettingsTest, SiteListEntry) {
  RunTest("settings/site_list_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteShortcutsPage) {
  RunTest("settings/site_shortcuts_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Slider) {
  RunTest("settings/settings_slider_test.js", "mocha.run()");
}


IN_PROC_BROWSER_TEST_F(SettingsTest, SpeedPage) {
  RunTest("settings/speed_page_test.js", "runMochaSuite('SpeedPage')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, CpuPerformanceOverride) {
  RunTest("settings/speed_page_test.js",
          "runMochaSuite('CpuPerformanceOverride')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, CpuPerformanceOverrideFeatureDisabled) {
  RunTest("settings/speed_page_test.js",
          "runMochaSuite('CpuPerformanceOverrideFeatureDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, OnStartupPage) {
  RunTest("settings/on_startup_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, StartupUrlsPage) {
  RunTest("settings/startup_urls_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, StorageAccessStaticSiteListEntry) {
  RunTest("settings/storage_access_static_site_list_entry_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, StorageAccessSiteListEntry) {
  RunTest("settings/storage_access_site_list_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, StorageAccessSiteList) {
  RunTest("settings/storage_access_site_list_test.js", "mocha.run()");
}
// Flaky on all OSes. TODO(crbug.com/40825327): Enable the test.
IN_PROC_BROWSER_TEST_F(SettingsTest, DISABLED_Subpage) {
  RunTest("settings/settings_subpage_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SyncAccountControl) {
  RunTest("settings/sync_account_control_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SyncEncryptionOptions) {
  RunTest("settings/sync_encryption_options_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, TabDiscardExceptionDialog) {
  RunTest("settings/tab_discard_exception_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ToggleButton) {
  RunTest("settings/settings_toggle_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ZoomLevels) {
  RunTest("settings/zoom_levels_test.js", "mocha.run()");
}

class SettingsSystemPageTest : public SettingsBrowserTest {
 private:
};

IN_PROC_BROWSER_TEST_F(SettingsSystemPageTest, SystemPage) {
  RunTest("settings/system_page_test.js", "mocha.run()");
}

using SettingsAllSitesTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsAllSitesTest, EnableRelatedWebsiteSets) {
  RunTest("settings/all_sites_test.js",
          "runMochaSuite('EnableRelatedWebsiteSets')");
}

IN_PROC_BROWSER_TEST_F(SettingsAllSitesTest, WithoutRelatedWebsiteSetsData) {
  RunTest("settings/all_sites_test.js",
          "runMochaSuite('WithoutRelatedWebsiteSetsData')");
}

using SettingsClearBrowsingDataTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsClearBrowsingDataTest,
                       DeleteBrowsingDataAccountIndicator) {
  RunTest("settings/clear_browsing_data_account_indicator_test.js",
          "runMochaSuite('DeleteBrowsingDataAccountIndicator')");
}

IN_PROC_BROWSER_TEST_F(SettingsClearBrowsingDataTest,
                       DeleteBrowsingDataTimePicker) {
  RunTest("settings/clear_browsing_data_time_picker_test.js",
          "runMochaSuite('DeleteBrowsingDataTimePicker')");
}

class SettingsCookiesPageTest : public SettingsBrowserTest {
 public:
  SettingsCookiesPageTest() {
    scoped_feature_list_.InitAndEnableFeature(
        privacy_sandbox::kRelatedWebsiteSetsUi);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, CookiesPageTest) {
  RunTest("settings/cookies_page_test.js", "runMochaSuite('CookiesPageTest')");
}

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, ExceptionsList) {
  RunTest("settings/cookies_page_test.js", "runMochaSuite('ExceptionsList')");
}

// Test with --enable-pixel-output-in-tests enabled, required by fingerprint
// element test using HTML canvas.
class SettingsWithPixelOutputTest : public SettingsBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
    SettingsBrowserTest::SetUpCommandLine(command_line);
  }
};

// https://crbug.com/40115579 - maybe flaky on Mac?
#if BUILDFLAG(IS_MAC)
#define MAYBE_FingerprintProgressArc DISABLED_FingerprintProgressArc
#else
#define MAYBE_FingerprintProgressArc FingerprintProgressArc
#endif
IN_PROC_BROWSER_TEST_F(SettingsWithPixelOutputTest,
                       MAYBE_FingerprintProgressArc) {
  RunTest("settings/security/fingerprint_progress_arc_test.js", "mocha.run()");
}

using SettingsLanguagePageTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsLanguagePageTest, AddLanguagesDialog) {
  RunTest("settings/languages_page_test.js",
          "runMochaSuite('LanguagesPage AddLanguagesDialog')");
}

IN_PROC_BROWSER_TEST_F(SettingsLanguagePageTest, LanguageMenu) {
  RunTest("settings/languages_page_test.js",
          "runMochaSuite('LanguagesPage LanguageMenu')");
}

IN_PROC_BROWSER_TEST_F(SettingsLanguagePageTest, MetricsBrowser) {
  RunTest("settings/languages_page_metrics_test_browser.js", "mocha.run()");
}

using SettingsPerformancePageTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsPerformancePageTest, TabDiscardExceptionList) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('TabDiscardExceptionList')");
}

IN_PROC_BROWSER_TEST_F(SettingsBrowserTest, DiscardIndicator) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('DiscardIndicator')");
}

IN_PROC_BROWSER_TEST_F(SettingsPerformancePageTest, PerformanceIntervention) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('PerformanceIntervention')");
}

using SettingsMemoryPageTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsMemoryPageTest, MemorySaver) {
  RunTest("settings/memory_page_test.js", "runMochaSuite('MemorySaver')");
}

IN_PROC_BROWSER_TEST_F(SettingsBrowserTest, MemorySaverAggressiveness) {
  RunTest("settings/memory_page_test.js",
          "runMochaSuite('MemorySaverAggressiveness')");
}

class SettingsPersonalizationOptionsTest : public SettingsBrowserTest {};

IN_PROC_BROWSER_TEST_F(SettingsPersonalizationOptionsTest, AllBuilds) {
  RunTest("settings/personalization_options_test.js",
          "runMochaSuite('AllBuilds')");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(SettingsPersonalizationOptionsTest, OfficialBuild) {
  RunTest("settings/personalization_options_test.js",
          "runMochaSuite('OfficialBuild')");
}
#endif

class SettingsPrivacyPageTest : public SettingsBrowserTest {
 protected:
  SettingsPrivacyPageTest() {
    scoped_feature_list1_.InitWithFeatures(
        {
        },
        {});
    scoped_feature_list2_.InitAndEnableFeatureWithParameters(
        features::kFedCm, {
                              {"DesktopSettings", "true"},
                          });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list1_;
  base::test::ScopedFeatureList scoped_feature_list2_;
};

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, PrivacyPage) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('PrivacyPage')");
}


IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       CookiesSubpageRedesignDisabled) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('CookiesSubpageRedesignDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, CookiesSubpage) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('CookiesSubpage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, PrivacyGuideRow) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('PrivacyGuideRow')");
}

// TODO(crbug.com/40710522): flaky failure on multiple platforms
IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       DISABLED_HappinessTrackingSurveys) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('HappinessTrackingSurveys')");
}

class SettingsNotificationsPageTest : public SettingsBrowserTest {};

IN_PROC_BROWSER_TEST_F(SettingsNotificationsPageTest, NotificationsPage) {
  RunTest("settings/notifications_page_test.js",
          "runMochaSuite('NotificationsPage')");
}

class SettingsGeolocationPageTest : public SettingsBrowserTest {};

IN_PROC_BROWSER_TEST_F(SettingsGeolocationPageTest, GeolocationPage) {
  RunTest("settings/geolocation_page_test.js",
          "runMochaSuite('GeolocationPage')");
}

class JavascriptOptimizerPageTest : public SettingsBrowserTest {};

IN_PROC_BROWSER_TEST_F(JavascriptOptimizerPageTest, JavascriptOptimizerPage) {
  RunTest("settings/v8_page_test.js", "runMochaSuite('V8Page')");
}

using SettingsRouteTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsRouteTest, Basic) {
  RunTest("settings/route_test.js", "runMochaSuite('Basic')");
}

IN_PROC_BROWSER_TEST_F(SettingsRouteTest, DynamicParameters) {
  RunTest("settings/route_test.js", "runMochaSuite('DynamicParameters')");
}

#define MAYBE_NonExistentRoute NonExistentRoute
IN_PROC_BROWSER_TEST_F(SettingsRouteTest, MAYBE_NonExistentRoute) {
  RunTest("settings/route_test.js", "runMochaSuite('NonExistentRoute')");
}

class SettingsSiteDetailsTest : public SettingsBrowserTest {};

// Disabling on debug due to flaky timeout on Win7 Tests (dbg)(1) bot.
// https://crbug.com/41378604 - later for other platforms in crbug.com/40106090.
// TODO(https://crbug.com/510377224): Re-enable test on windows
// TODO(crbug.com/543717125): Re-enable test on Mac.
#if !defined(NDEBUG) || BUILDFLAG(IS_MAC)
#define MAYBE_SiteDetails DISABLED_SiteDetails
#else
#define MAYBE_SiteDetails SiteDetails
#endif
IN_PROC_BROWSER_TEST_F(SettingsSiteDetailsTest, MAYBE_SiteDetails) {
  RunTest("settings/site_details_test.js", "mocha.run()");
}

class SettingsSiteListTest : public SettingsBrowserTest {};

// TODO(crbug.com/452036455): Disabled on Linux dbg due to flakiness.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_SiteList DISABLED_SiteList
#else
#define MAYBE_SiteList SiteList
#endif
IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, MAYBE_SiteList) {
  RunTest("settings/site_list_test.js", "runMochaSuite('SiteList')");
}

// TODO(crbug.com/41439813, crbug.com/40123519): Flaky test. When it is fixed,
// merge SiteListDisabled back into SiteList.
IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, DISABLED_SiteListDisabled) {
  RunTest("settings/site_list_test.js", "runMochaSuite('DISABLED_SiteList')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, SiteListEmbargoedOrigin) {
  RunTest("settings/site_list_test.js",
          "runMochaSuite('SiteListEmbargoedOrigin')");
}

// TODO(crbug.com/41439813): When the bug is fixed, merge
// SiteListCookiesExceptionTypes into SiteList.
IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, SiteListCookiesExceptionTypes) {
  RunTest("settings/site_list_test.js",
          "runMochaSuite('SiteListCookiesExceptionTypes')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, SiteListSearchTests) {
  RunTest("settings/site_list_test.js", "runMochaSuite('SiteListSearchTests')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, EditExceptionDialog) {
  RunTest("settings/site_list_test.js", "runMochaSuite('EditExceptionDialog')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, AddExceptionDialog) {
  RunTest("settings/site_list_test.js", "runMochaSuite('AddExceptionDialog')");
}

class SettingsSiteSettingsPageTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      content_settings::features::kSafetyCheckUnusedSitePermissions};
};

// TODO(crbug.com/40884439): Flaky.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_SiteSettingsPage DISABLED_SiteSettingsPage
#else
#define MAYBE_SiteSettingsPage SiteSettingsPage
#endif
IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest, MAYBE_SiteSettingsPage) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('SiteSettingsPage')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest,
                       ContentSettingsVisibility) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('ContentSettingsVisibility')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest, SiteSettingsList) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('SiteSettingsList')");
}

// Tests that the content settings page for Web Printing is not shown by
// default.
class SettingsSiteSettingsPageTestWithoutWebPrinting
    : public SettingsBrowserTest {};

IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest, SoundPage) {
  RunTest("settings/sound_page_test.js", "runMochaSuite('SoundPage')");
}

using YourSavedInfoTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(YourSavedInfoTest, AutofillAccount) {
  RunTest("settings/autofill_account_test.js", "mocha.run()");
}
