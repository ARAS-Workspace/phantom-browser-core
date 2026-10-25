// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_configurations.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/group_constants.h"
#include "components/segmentation_platform/public/features.h"

namespace {}  // namespace

namespace feature_engagement {

FeatureConfig CreateAlwaysTriggerConfig(const base::Feature* feature) {
  // Trim "IPH_" prefix from the feature name to use for trigger and used
  // events.
  const char* prefix = "IPH_";
  std::string stripped_feature_name = feature->name;
  if (base::StartsWith(stripped_feature_name, prefix,
                       base::CompareCase::SENSITIVE)) {
    stripped_feature_name = stripped_feature_name.substr(strlen(prefix));
  }

  // A config that always meets condition to trigger IPH.
  FeatureConfig config;
  config.valid = true;
  config.availability = Comparator(ANY, 0);
  config.session_rate = Comparator(ANY, 0);
  config.trigger = EventConfig(stripped_feature_name + "_trigger",
                               Comparator(ANY, 0), 90, 90);
  config.used =
      EventConfig(stripped_feature_name + "_used", Comparator(ANY, 0), 90, 90);
  return config;
}

std::optional<FeatureConfig> GetClientSideFeatureConfig(
    const base::Feature* feature) {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)
  if (kIPHProfileSwitchFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Show the promo once a year if the profile menu was not opened.
    config.trigger =
        EventConfig("profile_switch_trigger", Comparator(EQUAL, 0), 360, 360);
    config.used =
        EventConfig("profile_menu_shown", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHReadingListInSidePanelFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Show the promo once a year if the side panel was not opened.
    config.trigger =
        EventConfig("side_panel_trigger", Comparator(EQUAL, 0), 360, 360);
    config.used =
        EventConfig("side_panel_shown", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHReadingModeSidePanelFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    // Show the promo up to 3 times a year.
    config.trigger = EventConfig("iph_reading_mode_side_panel_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.used = EventConfig("reading_mode_side_panel_shown",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHSidePanelGenericPinnableFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show the promo once a year if the side panel was not opened.
    config.trigger = EventConfig("side_panel_pinnable_trigger",
                                 Comparator(EQUAL, 0), 360, 360);
    config.used = EventConfig(feature_engagement::events::kSidePanelPinned,
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHGMCCastStartStopFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("gmc_start_stop_iph_trigger",
                                 Comparator(EQUAL, 0), 180, 180);
    config.used = EventConfig("media_route_stopped_from_gmc",
                              Comparator(EQUAL, 0), 180, 180);
    return config;
  }

  if (kIPHGMCLocalMediaCastingFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.trigger = EventConfig("gmc_local_media_cast_iph_trigger",
                                 Comparator(EQUAL, 0), 180, 180);
    config.used = EventConfig("media_route_started_from_gmc",
                              Comparator(EQUAL, 0), 180, 180);

    return config;
  }

  if (kIPHDesktopSharedHighlightingFeature.name == feature->name) {
    // A config that allows the shared highlighting desktop IPH to be shown
    // when a user receives a highlight:
    // * Once per 7 days
    // * Up to 5 times but only if unused in the last 7 days.
    // * Used fewer than 2 times

    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger = EventConfig("iph_desktop_shared_highlighting_trigger",
                                 Comparator(LESS_THAN, 5), 360, 360);
    config.used = EventConfig("iph_desktop_shared_highlighting_used",
                              Comparator(LESS_THAN, 2), 360, 360);
    config.event_configs.insert(
        EventConfig("iph_desktop_shared_highlighting_trigger",
                    Comparator(EQUAL, 0), 7, 360));
    return config;
  }

  if (kIPHBatterySaverModeFeature.name == feature->name) {
    // Show promo once a year when the battery saver toolbar icon is visible.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("battery_saver_info_triggered",
                                 Comparator(LESS_THAN, 1), 360, 360);
    config.used =
        EventConfig("battery_saver_info_shown", Comparator(EQUAL, 0), 7, 360);
    return config;
  }

  if (kIPHMemorySaverModeFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Show the promo max 3 times, once per week.
    config.trigger = EventConfig("high_efficiency_prompt_in_trigger",
                                 Comparator(LESS_THAN, 1), 7, 360);
    // This event is never logged but is included for consistency.
    config.used = EventConfig("high_efficiency_prompt_in_used",
                              Comparator(EQUAL, 0), 360, 360);
    config.event_configs.insert(EventConfig("high_efficiency_prompt_in_trigger",
                                            Comparator(LESS_THAN, 1), 360,
                                            360));
    return config;
  }

  if (kIPHPerformanceInterventionDialogFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show intervention dialog at most 3 times per day and no more than 21
    // times per week.
    config.trigger = EventConfig("performance_intervention_dialog_trigger",
                                 Comparator(LESS_THAN, 3), 1, 360);
    config.event_configs.insert(
        EventConfig("performance_intervention_dialog_trigger",
                    Comparator(LESS_THAN, 21), 7, 360));
    return config;
  }

  if (kIPHPowerBookmarksSidePanelFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("iph_power_bookmarks_side_panel_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.used = EventConfig("power_bookmarks_side_panel_shown",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHSmartTabSharingTryItFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Show the promo max 3 times total (per year), once per week.
    config.trigger = EventConfig("smart_tab_sharing_try_it_trigger",
                                 Comparator(EQUAL, 0), 7, 7);
    config.event_configs.insert(EventConfig("smart_tab_sharing_try_it_trigger",
                                            Comparator(LESS_THAN, 3), 360,
                                            360));
    config.used = EventConfig("smart_tab_sharing_try_it_used",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHSmartTabSharingDefaultOnFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Show the promo max 3 times total (per year), once per week.
    // Repeated activation of smart tab sharing is also required.
    config.trigger = EventConfig("smart_tab_sharing_default_on_trigger",
                                 Comparator(EQUAL, 0), 7, 7);
    config.event_configs.insert(
        EventConfig("smart_tab_sharing_default_on_trigger",
                    Comparator(LESS_THAN, 3), 360, 360));
    config.event_configs.insert(
        EventConfig("smart_tab_sharing_activated",
                    Comparator(GREATER_THAN_OR_EQUAL, 3), 360, 360));
    config.used = EventConfig("smart_tab_sharing_default_on_used",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (kIPHExtensionsMenuFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(LESS_THAN, 1);

    // Show promo up to three times a year or until the extensions menu is
    // opened while any extension has access to the current site.
    config.trigger = EventConfig("extensions_menu_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.used =
        EventConfig("extensions_menu_opened_while_extension_has_access",
                    Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHExtensionsRequestAccessButtonFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(LESS_THAN, 1);

    // Show promo up to three times a year or until the request access button
    // is clicked.
    config.trigger = EventConfig("extensions_request_access_button_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.used = EventConfig("extensions_request_access_button_clicked",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  if (kIPHCompanionSidePanelFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;

    // Show the promo up to 3 times a year.
    config.trigger = EventConfig("iph_companion_side_panel_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.used =
        EventConfig("companion_side_panel_accessed_via_toolbar_button",
                    Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHCompanionSidePanelRegionSearchFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    // Show the promo up to 3 times a year.
    config.trigger =
        EventConfig("iph_companion_side_panel_region_search_trigger",
                    Comparator(LESS_THAN, 3), 360, 360);
    config.used =
        EventConfig("companion_side_panel_region_search_button_clicked",
                    Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHDiscardRingFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("discard_ring_trigger", Comparator(EQUAL, 0), 360, 360);
    // This event is never logged but is included for consistency.
    config.used =
        EventConfig("discard_ring_used", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHBackNavigationMenuFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("back_navigation_menu_iph_is_triggered",
                                 Comparator(LESS_THAN_OR_EQUAL, 4), 360, 360);
    config.used = EventConfig("back_navigation_menu_is_opened",
                              Comparator(EQUAL, 0), 7, 360);
    config.snooze_params.snooze_interval = 7;
    config.snooze_params.max_limit = 4;
    return config;
  }

  if (kIPHComposeNewBadgeFeature.name == feature->name) {
    // A config that allows the new badge displayed in the Compose feature nudge
    // to be shown at most 4 times in a 10-day window and only while the user
    // has opened the Compose feature less than 3 times.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger = EventConfig("compose_new_badge_triggered",
                                 Comparator(LESS_THAN, 4), 10, 360);
    config.used =
        EventConfig("compose_activated", Comparator(LESS_THAN, 3), 360, 360);
    return config;
  }

  if (kIPHComposeMSBBSettingsFeature.name == feature->name) {
    // A config that allows a toast to be displayed in the Settings page when
    // opened via the Compose MSBB feature
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.trigger = EventConfig("compose_msbb_settings_feature_trigger",
                                 Comparator(ANY, 0), 90, 90);
    config.used = EventConfig("compose_msbb_settings_feature_used",
                              Comparator(ANY, 0), 90, 90);
    return config;
  }

  if (kIPHAutofillCreditCardBenefitFeature.name == feature->name) {
    // The credit card benefit IPH appears up to three times over 10 years and
    // only once per session. Dismissing it stops it from showing again.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(LESS_THAN, 1);
    config.trigger = EventConfig("autofill_credit_card_benefit_iph_trigger",
                                 Comparator(LESS_THAN, 3), 90, 360);
    config.used =
        EventConfig("autofill_credit_card_benefit_iph_accepted",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHAutofillExternalAccountProfileSuggestionFeature.name ==
      feature->name) {
    // Externally created account profile suggestion IPH is shown:
    // * once for an installation, 10-year window is used as the maximum
    // * if there was no address keyboard accessory IPH in the last 2 weeks
    // * if such a suggestion was not already accepted
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("autofill_external_account_profile_suggestion_iph_trigger",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.used =
        EventConfig("autofill_external_account_profile_suggestion_accepted",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);

    return config;
  }

  if (kIPHAutofillVirtualCardSuggestionFeature.name == feature->name) {
    // A config that allows the virtual card credit card suggestion IPH to be
    // shown when:
    // * it has been shown less than three times in last 90 days;
    // * the virtual card suggestion has been selected less than twice in last
    // 90 days.

    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("autofill_virtual_card_iph_trigger",
                                 Comparator(LESS_THAN, 3), 90, 360);
    config.used = EventConfig("autofill_virtual_card_suggestion_accepted",
                              Comparator(LESS_THAN, 2), 90, 360);

    return config;
  }

  if (kIPHAutofillVirtualCardCVCSuggestionFeature.name == feature->name) {
    // A config that allows the virtual card CVC suggestion IPH to be
    // shown when:
    // * it has been shown less than three times in last 90 days;
    // * the virtual card CVC suggestion has been selected less than twice in
    // last 90 days.

    FeatureConfig config;
    config.valid = true;
    // On desktop, toasts should always be available.
    config.availability = Comparator(ANY, 0);
    config.trigger = EventConfig("autofill_virtual_card_cvc_iph_trigger",
                                 Comparator(LESS_THAN, 3), 90, 360);
    config.used = EventConfig("autofill_virtual_card_cvc_suggestion_accepted",
                              Comparator(LESS_THAN, 2), 90, 360);

    // This promo blocks specific promos in the same session.
    config.session_rate_impact.type = SessionRateImpact::Type::EXPLICIT;
    config.session_rate_impact.affected_features.emplace();
    config.session_rate_impact.affected_features->push_back(
        "IPH_AutofillVirtualCardSuggestion");

    return config;
  }

#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX)

  if (kIPHDummyFeature.name == feature->name) {
    // Only used for tests. Various magic tricks are used below to ensure this
    // config is invalid and unusable.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(LESS_THAN, 0);
    config.session_rate = Comparator(LESS_THAN, 0);
    config.trigger = EventConfig("dummy_feature_iph_trigger",
                                 Comparator(LESS_THAN, 0), 1, 1);
    config.used =
        EventConfig("dummy_feature_action", Comparator(LESS_THAN, 0), 1, 1);
    return config;
  }

  return std::nullopt;
}

}  // namespace feature_engagement
