// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/values_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/browser/new_tab_page/modules/modules_constants.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/util/webui_util_desktop.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/omnibox/browser/omnibox.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sync/service/sync_service.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/native_theme/native_theme.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_utils.h"
#include "components/user_education/webui/help_bubble_handler.h"  // nogncheck
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace {

constexpr char kDisableInteraction[] = "disable";
constexpr char kDismissInteraction[] = "dismiss";
constexpr char kIgnoreInteraction[] = "ignore";
constexpr char kUseInteraction[] = "use";

constexpr auto kModuleInteractionNames =
    base::MakeFixedFlatSet<std::string_view>(
        {kDisableInteraction, kDismissInteraction, kIgnoreInteraction,
         kUseInteraction});

// Returns a list of module IDs that are eligible for HATS.
std::vector<std::string> GetSurveyEligibleModuleIds() {
  return base::SplitString(
      base::GetFieldTrialParamValueByFeature(
          features::kHappinessTrackingSurveysForDesktopNtpModules,
          ntp_features::kNtpModulesEligibleForHappinessTrackingSurveyParam),
      ",:;", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
}

new_tab_page::mojom::ThemePtr MakeTheme(
    const ui::ColorProvider& color_provider,
    const ui::ThemeProvider* theme_provider,
    ThemeService* theme_service,
    NtpCustomBackgroundService* ntp_custom_background_service,
    content::WebContents* web_contents) {
  if (ntp_custom_background_service) {
    ntp_custom_background_service->RefreshBackgroundIfNeeded();
  }
  auto theme = new_tab_page::mojom::Theme::New();
  auto most_visited = most_visited::mojom::MostVisitedTheme::New();
  auto custom_background =
      ntp_custom_background_service
          ? ntp_custom_background_service->GetCustomBackground()
          : std::nullopt;
  theme->background_color = color_provider.GetColor(kColorNewTabPageBackground);
  theme->is_baseline = theme_service->GetIsBaseline();
  // Theme is GM3 if there is a GM3 color set or the theme is baseline and no
  // CWS theme is set.
  theme->is_gm3 =
      (theme_service->GetUserColor().has_value() || theme->is_baseline) &&
      !theme_service->UsingExtensionTheme();
  const bool theme_has_custom_image =
      theme_provider &&
      theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND);
  SkColor text_color;
  if (custom_background.has_value()) {
    text_color = color_provider.GetColor(kColorNewTabPageTextUnthemed);
    most_visited->background_color =
        color_provider.GetColor(kColorNewTabPageMostVisitedTileBackground);
  } else if (theme_has_custom_image) {
    text_color = color_provider.GetColor(kColorNewTabPageTextUnthemed);
    most_visited->background_color =
        color_provider.GetColor(kColorNewTabPageMostVisitedTileBackground);
  } else {
    text_color = color_provider.GetColor(kColorNewTabPageText);
    most_visited->background_color =
        color_provider.GetColor(kColorNewTabPageMostVisitedTileBackground);
  }

  most_visited->use_white_tile_icon =
      color_utils::IsDark(most_visited->background_color);
  most_visited->is_dark = !color_utils::IsDark(text_color);
  theme->text_color = text_color;
  theme->is_dark =
      !color_utils::IsDark(color_provider.GetColor(kColorNewTabPageText));
  auto background_image = new_tab_page::mojom::BackgroundImage::New();
  if (theme_has_custom_image) {
    if (theme_service->UsingExtensionTheme()) {
      background_image->image_source =
          new_tab_page::mojom::NtpBackgroundImageSource::kThirdPartyTheme;
    }
    theme->is_custom_background = false;
    auto theme_id = theme_service->GetThemeID();
    background_image->url = GURL(base::StrCat(
        {"chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND?", theme_id}));
    background_image->url_2x = GURL(base::StrCat(
        {"chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND@2x?", theme_id}));
    if (theme_provider->HasCustomImage(IDR_THEME_NTP_ATTRIBUTION)) {
      background_image->attribution_url = GURL(base::StrCat(
          {"chrome://theme/IDR_THEME_NTP_ATTRIBUTION?", theme_id}));
    }
    background_image->size = "initial";
    switch (theme_provider->GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_TILING)) {
      case ThemeProperties::NO_REPEAT:
        background_image->repeat_x = "no-repeat";
        background_image->repeat_y = "no-repeat";
        break;
      case ThemeProperties::REPEAT_X:
        background_image->repeat_x = "repeat";
        background_image->repeat_y = "no-repeat";
        break;
      case ThemeProperties::REPEAT_Y:
        background_image->repeat_x = "no-repeat";
        background_image->repeat_y = "repeat";
        break;
      case ThemeProperties::REPEAT:
        background_image->repeat_x = "repeat";
        background_image->repeat_y = "repeat";
        break;
    }
    int alignment = theme_provider->GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
    if (alignment & ThemeProperties::ALIGN_LEFT) {
      background_image->position_x = "left";
    } else if (alignment & ThemeProperties::ALIGN_RIGHT) {
      background_image->position_x = "right";
    } else {
      background_image->position_x = "center";
    }
    if (alignment & ThemeProperties::ALIGN_TOP) {
      background_image->position_y = "top";
    } else if (alignment & ThemeProperties::ALIGN_BOTTOM) {
      background_image->position_y = "bottom";
    } else {
      background_image->position_y = "center";
    }
  } else if (custom_background.has_value()) {
    theme->is_custom_background = true;
    background_image->url = custom_background->custom_background_url;
    new_tab_page::mojom::NtpBackgroundImageSource image_source = new_tab_page::
        mojom::NtpBackgroundImageSource::kFirstPartyThemeWithoutDailyRefresh;
    if (custom_background->daily_refresh_enabled) {
      image_source = new_tab_page::mojom::NtpBackgroundImageSource::
          kFirstPartyThemeWithDailyRefresh;
    } else if (custom_background->local_background_id.has_value()) {
      if (custom_background->is_inspiration_image) {
        image_source = new_tab_page::mojom::NtpBackgroundImageSource::
            kWallpaperSearchInspiration;
      } else {
        image_source =
            new_tab_page::mojom::NtpBackgroundImageSource::kWallpaperSearch;
      }
    } else if (custom_background->is_uploaded_image) {
      image_source =
          new_tab_page::mojom::NtpBackgroundImageSource::kUploadedImage;
    }
    background_image->image_source = image_source;
  } else {
    background_image = nullptr;
  }

  theme->background_image = std::move(background_image);
  if (custom_background.has_value() && !theme_has_custom_image) {
    theme->background_image_attribution_1 =
        custom_background->custom_background_attribution_line_1;
    theme->background_image_attribution_2 =
        custom_background->custom_background_attribution_line_2;
    theme->background_image_attribution_url =
        custom_background->custom_background_attribution_action_url;
    theme->background_image_collection_id = custom_background->collection_id;
    theme->daily_refresh_enabled = custom_background->daily_refresh_enabled;
  }

  theme->most_visited = std::move(most_visited);

  return theme;
}

base::DictValue MakeModuleInteractionTriggerIdDictionary() {
  const auto data = base::GetFieldTrialParamValueByFeature(
      features::kHappinessTrackingSurveysForDesktopNtpModules,
      ntp_features::kNtpModulesInteractionBasedSurveyEligibleIdsParam);
  if (data.empty()) {
    return base::DictValue();
  }

  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      data, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  if (!value_with_error.has_value()) {
    LOG(ERROR)
        << "Failed to parse "
           "ntp_features::kNtpModulesInteractionBasedSurveyEligibleIdsParam ("
        << value_with_error.error().message << ") on line "
        << value_with_error.error().line << " at position "
        << value_with_error.error().column;
    return base::DictValue();
  }

  if (!value_with_error->is_dict()) {
    LOG(WARNING)
        << "ntp_features::kNtpModulesInteractionBasedSurveyEligibleIdsParam "
           "data skipped. Not a dictionary.";
    return base::DictValue();
  }

  return std::move(*value_with_error).TakeDict();
}

}  // namespace

// static
const char NewTabPageHandler::kModuleDismissedHistogram[] =
    "NewTabPage.Modules.Dismissed";
const char NewTabPageHandler::kModuleRestoredHistogram[] =
    "NewTabPage.Modules.Restored";

NewTabPageHandler::NewTabPageHandler(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
    Profile* profile,
    NtpCustomBackgroundService* ntp_custom_background_service,
    ThemeService* theme_service,
    syncer::SyncService* sync_service,
    segmentation_platform::SegmentationPlatformService*
        segmentation_platform_service,
    content::WebContents* web_contents,
    const base::Time& ntp_navigation_start_time,
    base::TimeTicks ntp_navigation_start_time_ticks,
    const std::vector<ntp::ModuleIdDetail>* module_id_details)
    : SettingsEnabledObserver(
          optimization_guide::UserVisibleFeatureKey::kWallpaperSearch),
      logger_(profile,
              chrome::ChromeUINewTabPageURLAsGURL(),
              ntp_navigation_start_time_ticks),
      ntp_custom_background_service_(ntp_custom_background_service),
      theme_provider_(webui::GetThemeProviderDeprecated(web_contents)),
      theme_service_(theme_service),
      sync_service_(sync_service),
      segmentation_platform_service_(segmentation_platform_service),
      profile_(profile),
      web_contents_(web_contents),
      feature_promo_helper_(std::make_unique<NewTabPageFeaturePromoHelper>()),
      ntp_navigation_start_time_(ntp_navigation_start_time),
      module_id_details_(module_id_details),
      microsoft_auth_service_(
          MicrosoftAuthServiceFactory::GetForProfile(profile)),
      interaction_module_id_trigger_dict_(
          MakeModuleInteractionTriggerIdDictionary()),
      browser_window_changed_subscription_(
          webui::RegisterBrowserWindowInterfaceChanged(
              web_contents_,
              base::BindRepeating(
                  &NewTabPageHandler::OnBrowserWindowInterfaceChanged,
                  base::Unretained(this)))),
      page_{std::move(pending_page)},
      receiver_{this, std::move(pending_page_handler)} {
  CHECK(ntp_custom_background_service_);
  CHECK(web_contents_);
  CHECK(theme_service_);
  CHECK(feature_promo_helper_);
  theme_service_observation_.Observe(theme_service_.get());
  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  ntp_custom_background_service_observation_.Observe(
      ntp_custom_background_service_.get());
  if (microsoft_auth_service_) {
    microsoft_auth_service_->AddObserver(this);
  }
  if (customize_chrome::IsWallpaperSearchEnabledForProfile(profile_)) {
    optimization_guide_keyed_service_ =
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
    if (optimization_guide_keyed_service_) {
      optimization_guide_keyed_service_
          ->AddModelExecutionSettingsEnabledObserver(this);
    }
  }

  OnBrowserWindowInterfaceChanged();

  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpBackgroundImageErrorDetection)) {
    ntp_custom_background_service_->VerifyCustomBackgroundImageURL();
  } else {
    OnThemeChanged();
  }

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNtpModulesVisible,
      base::BindRepeating(&NewTabPageHandler::UpdateDisabledModules,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNtpDisabledModules,
      base::BindRepeating(&NewTabPageHandler::UpdateDisabledModules,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNtpHiddenModules,
      base::BindRepeating(&NewTabPageHandler::UpdateDisabledModules,
                          base::Unretained(this)));

  pref_change_registrar_.Add(
      prefs::kSeedColorChangeCount,
      base::BindRepeating(&NewTabPageHandler::MaybeShowWebstoreToast,
                          base::Unretained(this)));

  if (base::FeatureList::IsEnabled(
          feature_engagement::kIPHDesktopRealboxContextualSearchFeature)) {
    searchbox_shown_subscription_ =
        ui::ElementTracker::GetElementTracker()
            ->AddElementShownInAnyContextCallback(
                NewTabPageUI::kRealboxContextualEntrypointElementId,
                base::BindRepeating(
                    &NewTabPageHandler::TryShowRealboxContextualMenuIPH,
                    weak_ptr_factory_.GetWeakPtr()));
  }
}

NewTabPageHandler::~NewTabPageHandler() {
  if (optimization_guide_keyed_service_) {
    optimization_guide_keyed_service_
        ->RemoveModelExecutionSettingsEnabledObserver(this);
    optimization_guide_keyed_service_ = nullptr;
  }
  if (microsoft_auth_service_) {
    microsoft_auth_service_->RemoveObserver(this);
  }
}

// static
void NewTabPageHandler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kNtpDisabledModules);
  registry->RegisterListPref(prefs::kNtpHiddenModules);
  registry->RegisterListPref(prefs::kNtpModulesOrder);
  registry->RegisterBooleanPref(prefs::kNtpModulesVisible, true);
  registry->RegisterBooleanPref(prefs::kNtpToolChipsVisible, true);
  registry->RegisterIntegerPref(prefs::kNtpCustomizeChromeButtonOpenCount, 0);
  registry->RegisterDictionaryPref(prefs::kNtpModulesInteractedCountDict);
  registry->RegisterDictionaryPref(prefs::kNtpModulesLoadedCountDict);
  registry->RegisterIntegerPref(prefs::kNtpWallpaperSearchButtonShownCount, 0);
  registry->RegisterBooleanPref(prefs::kNtpOutlookModuleVisible, false);
  registry->RegisterBooleanPref(prefs::kNtpSharepointModuleVisible, false);
  registry->RegisterIntegerPref(
      prefs::kNtpCustomizeChromeSidePanelAutoOpeningsCount, 0);
  registry->RegisterBooleanPref(prefs::kNtpCustomizeChromeExplicitlyClosed,
                                false);
  registry->RegisterBooleanPref(prefs::kNtpCustomizeChromeIPHAutoOpened, false);
}

void NewTabPageHandler::SetMostVisitedSettings(ntp_tiles::TileType type,
                                               bool visible) {
  bool old_visible = IsShortcutsVisible();
  if (old_visible != visible) {
    profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, visible);
    LogEvent(NTP_CUSTOMIZE_SHORTCUT_TOGGLE_VISIBILITY);
  }

  ntp_tiles::TileType old_type = GetTileType();
  if (old_type != type) {
    profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible,
                                     type == ntp_tiles::TileType::kCustomLinks);
    profile_->GetPrefs()->SetBoolean(
        ntp_prefs::kNtpEnterpriseShortcutsVisible,
        type == ntp_tiles::TileType::kEnterpriseShortcuts);
    LogEvent(NTP_CUSTOMIZE_SHORTCUT_TOGGLE_TYPE);
  }
}

void NewTabPageHandler::GetMostVisitedSettings(
    GetMostVisitedSettingsCallback callback) {
  ntp_tiles::TileType type = GetTileType();
  bool visible = IsShortcutsVisible();
  std::move(callback).Run(type, visible);
}

void NewTabPageHandler::OnDismissModule(const std::string& module_id) {
  const std::string histogram_prefix(kModuleDismissedHistogram);
  base::UmaHistogramExactLinear(histogram_prefix, 1, 1);
  base::UmaHistogramExactLinear(histogram_prefix + "." + module_id, 1, 1);

  RecordModuleInteraction(module_id);
  MaybeLaunchInteractionSurvey(kDismissInteraction, module_id);
}

void NewTabPageHandler::OnRestoreModule(const std::string& module_id) {
  const std::string histogram_prefix(kModuleRestoredHistogram);
  base::UmaHistogramExactLinear(histogram_prefix, 1, 1);
  base::UmaHistogramExactLinear(histogram_prefix + "." + module_id, 1, 1);
}

void NewTabPageHandler::SetModulesVisible(bool visible) {
  DisableModuleAutoRemoval(profile_, ntp_modules::kAllModulesId);
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, visible);
}

void NewTabPageHandler::SetModulesDisabled(
    const std::vector<std::string>& module_ids,
    bool disabled,
    bool is_user_action) {
  if (module_ids.empty()) {
    return;
  }

  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpDisabledModules);
  base::ListValue& list = update.Get();
  for (const auto& module_id : module_ids) {
    if (disabled) {
      if (!list.contains(module_id)) {
        list.Append(module_id);
      }
    } else {
      list.EraseValue(base::Value(module_id));
    }
  }

  DisableModuleListAutoRemoval(profile_, module_ids);

  // We're not recording a user interaction if the modules were disabled due to
  // feature optimization auto removal.
  if (is_user_action) {
    for (const auto& module_id : module_ids) {
      IncrementDictPrefKeyCount(prefs::kNtpModulesInteractedCountDict,
                                module_id);
      MaybeLaunchInteractionSurvey(kDisableInteraction, module_id);
    }
  }
}

void NewTabPageHandler::UpdateDisabledModules() {
  std::set<std::string> module_ids_set;
  // If the module visibility is managed by policy we either disable all modules
  // (if invisible) or no modules (if visible).
  if (!profile_->GetPrefs()->IsManagedPreference(prefs::kNtpModulesVisible)) {
    const auto& disabled_module_ids_value =
        profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules);
    for (const auto& id : disabled_module_ids_value) {
      module_ids_set.insert(id.GetString());
    }
  }

  // Hidden modules should be respected as long as modules are visible.
  if (profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesVisible)) {
    const auto& hidden_module_ids_value =
        profile_->GetPrefs()->GetList(prefs::kNtpHiddenModules);
    for (const auto& id : hidden_module_ids_value) {
      module_ids_set.insert(id.GetString());
    }
  }

  std::vector<std::string> module_ids(module_ids_set.begin(),
                                      module_ids_set.end());
  page_->SetDisabledModules(
      !profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesVisible),
      std::move(module_ids));
}

void NewTabPageHandler::OnModulesLoadedWithData(
    const std::vector<std::string>& module_ids) {
  UpdateModulesStaleness(profile_, module_ids);

  for (const auto& module_id : module_ids) {
    IncrementDictPrefKeyCount(prefs::kNtpModulesLoadedCountDict, module_id);
  }

  std::vector<std::string> survey_eligible_module_ids =
      GetSurveyEligibleModuleIds();
  if (std::any_of(module_ids.begin(), module_ids.end(),
                  [&survey_eligible_module_ids](std::string id) {
                    return std::ranges::contains(survey_eligible_module_ids,
                                                 id);
                  })) {
    HatsService* hats_service = HatsServiceFactory::GetForProfile(
        profile_, /*create_if_necessary=*/true);
    CHECK(hats_service);
    hats_service->LaunchDelayedSurveyForWebContents(
        kHatsSurveyTriggerNtpModules, web_contents_, 0);
    return;
  }

  const auto module_ignored_criteria_threshold =
      base::GetFieldTrialParamByFeatureAsInt(
          features::kHappinessTrackingSurveysForDesktopNtpModules,
          ntp_features::kNtpModuleIgnoredCriteriaThreshold, 25);
  for (const auto& module_id : module_ids) {
    const base::DictValue& interacted_counts_dict =
        profile_->GetPrefs()->GetDict(prefs::kNtpModulesInteractedCountDict);
    std::optional<int> interacted_count =
        interacted_counts_dict.FindInt(module_id);
    if (interacted_count.value_or(0) != 0) {
      continue;
    }

    const base::DictValue& loaded_counts_dict =
        profile_->GetPrefs()->GetDict(prefs::kNtpModulesLoadedCountDict);
    std::optional<int> loaded_count = loaded_counts_dict.FindInt(module_id);
    if (loaded_count.value_or(0) >= module_ignored_criteria_threshold) {
      const auto survey_delay_time_ms = base::GetFieldTrialParamByFeatureAsInt(
          features::kHappinessTrackingSurveysForDesktopNtpModules,
          ntp_features::kNtpModuleIgnoredHaTSDelayTimeParam, 0);
      MaybeLaunchInteractionSurvey(kIgnoreInteraction, module_id,
                                   survey_delay_time_ms);
      break;
    }
  }
}

void NewTabPageHandler::OnModuleUsed(const std::string& module_id) {
  RecordModuleInteraction(module_id);
  MaybeLaunchInteractionSurvey(kUseInteraction, module_id);
}

void NewTabPageHandler::GetModulesIdNames(GetModulesIdNamesCallback callback) {
  std::vector<new_tab_page::mojom::ModuleIdNamePtr> modules_details;
  for (const auto& module_id_detail : *module_id_details_) {
    auto module_id_name = new_tab_page::mojom::ModuleIdName::New();
    module_id_name->id = module_id_detail.id_;
    module_id_name->name =
        l10n_util::GetStringUTF8(module_id_detail.name_message_id_);
    modules_details.push_back(std::move(module_id_name));
  }

  std::move(callback).Run(std::move(modules_details));
}

void NewTabPageHandler::GetModulesEligibleForRemoval(
    GetModulesEligibleForRemovalCallback callback) {
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::vector<std::string>());

  // (1) Skip modules removal if the feature flag is not enabled.
  std::vector<std::string> removal_eligible_module_ids;
  if (!base::FeatureList::IsEnabled(
          ntp_features::kNtpFeatureOptimizationModuleRemoval)) {
    return;
  }

  // (2) Skip modules removal if it's a managed preference.
  if (profile_->GetPrefs()->IsManagedPreference(prefs::kNtpModulesVisible)) {
    return;
  }

  // (3) Skip modules removal if it's force disabled for all modules.
  const base::DictValue& module_removal_disabled_dict =
      profile_->GetPrefs()->GetDict(
          ntp_prefs::kNtpModulesAutoRemovalDisabledDict);
  const bool is_all_module_removal_disabled =
      module_removal_disabled_dict.FindBool(ntp_modules::kAllModulesId)
          .value_or(false);

  if (is_all_module_removal_disabled) {
    return;
  }

  // (4) Otherwise, we check for each module if the module auto removal is not
  // force disabled and if the staleness count is above the threshold.
  const int staleness_threshold =
      ntp_features::kStaleModulesCountThreshold.Get();
  const base::DictValue& staleness_counts_dict =
      profile_->GetPrefs()->GetDict(ntp_prefs::kNtpModuleStalenessCountDict);

  for (const auto& module_id_detail : *module_id_details_) {
    const bool is_module_removal_disabled =
        module_removal_disabled_dict.FindBool(module_id_detail.id_)
            .value_or(false);
    const int staleness_count =
        staleness_counts_dict.FindInt(module_id_detail.id_).value_or(0);
    const bool is_above_threshold = staleness_count >= staleness_threshold;
    if (!is_module_removal_disabled && is_above_threshold) {
      removal_eligible_module_ids.push_back(module_id_detail.id_);
    }
  }

  return std::move(callback).Run(std::move(removal_eligible_module_ids));
}

void NewTabPageHandler::SetModulesOrder(
    const std::vector<std::string>& module_ids) {
  base::ListValue module_ids_value;
  for (const auto& module_id : module_ids) {
    module_ids_value.Append(module_id);
  }
  profile_->GetPrefs()->SetList(prefs::kNtpModulesOrder,
                                std::move(module_ids_value));
}

void NewTabPageHandler::GetModulesOrder(GetModulesOrderCallback callback) {
  std::vector<std::string> module_ids;

  // First, apply order as set by the last drag&drop interaction.
  if (base::FeatureList::IsEnabled(ntp_features::kNtpModulesDragAndDrop)) {
    const auto& module_ids_value =
        profile_->GetPrefs()->GetList(prefs::kNtpModulesOrder);
    for (const auto& id : module_ids_value) {
      module_ids.push_back(id.GetString());
    }
  }

  // Second, append Finch order for modules _not_ ordered by drag&drop.
  std::ranges::copy_if(ntp_features::GetModulesOrder(),
                       std::back_inserter(module_ids),
                       [&module_ids](const std::string& id) {
                         return !std::ranges::contains(module_ids, id);
                       });

  // Third, append default module order for any modules not ordered by
  // drag&drop or Finch.
  std::ranges::copy_if(ntp_modules::kOrderedModuleIds,
                       std::back_inserter(module_ids),
                       [&module_ids](const std::string& id) {
                         return !std::ranges::contains(module_ids, id);
                       });

  std::move(callback).Run(std::move(module_ids));
}

void NewTabPageHandler::UpdateModulesLoadable() {
  if (!microsoft_auth_service_ || SyncMicrosoftModulesWithAuth()) {
    page_->SetModulesLoadable();
  }
}

void NewTabPageHandler::UpdateFooterVisibility() {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    return;
  }

  // TODO(webium): FooterController depends on BrowserView, but WebUIBrowser
  // doesn't have a BrowserView.
  if (webui_browser::IsWebUIBrowserEnabled()) {
    return;
  }

  auto* browser = webui::GetBrowserWindowInterface(web_contents_);
  if (!browser) {
    // TODO(crbug.com/378475391): NTP should always load into a WebContents
    // owned by a TabModel. Remove this once NTP loading has been restricted to
    // browser tabs only.
    return;
  }

  auto* footer_controller = browser->GetFeatures().new_tab_footer_controller();
  CHECK(footer_controller);
  OnFooterVisibilityUpdated(footer_controller->GetFooterVisible(web_contents_));
}

void NewTabPageHandler::OnAppRendered(double time) {
  LogEvent(NTP_APP_RENDERED, base::Time::FromMillisecondsSinceUnixEpoch(time) -
                                 ntp_navigation_start_time_);
}

void NewTabPageHandler::OnCustomizeDialogAction(
    new_tab_page::mojom::CustomizeDialogAction action) {
  NTPLoggingEventType event;
  switch (action) {
    case new_tab_page::mojom::CustomizeDialogAction::kCancelClicked:
      event = NTP_CUSTOMIZATION_MENU_CANCEL;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kDoneClicked:
      event = NTP_CUSTOMIZATION_MENU_DONE;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kOpenClicked:
      event = NTP_CUSTOMIZATION_MENU_OPENED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kBackgroundsBackClicked:
      event = NTP_BACKGROUND_BACK_CLICK;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsNoBackgroundSelected:
      event = NTP_BACKGROUND_DEFAULT_SELECTED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsCollectionOpened:
      event = NTP_BACKGROUND_OPEN_COLLECTION;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsRefreshToggleClicked:
      event = NTP_BACKGROUND_REFRESH_TOGGLE_CLICKED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kBackgroundsImageSelected:
      event = NTP_BACKGROUND_SELECT_IMAGE;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsUploadFromDeviceClicked:
      event = NTP_BACKGROUND_UPLOAD_FROM_DEVICE;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kShortcutsCustomLinksClicked:
      event = NTP_CUSTOMIZE_SHORTCUT_CUSTOM_LINKS_CLICKED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kShortcutsMostVisitedClicked:
      event = NTP_CUSTOMIZE_SHORTCUT_MOST_VISITED_CLICKED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kShortcutsVisibilityToggleClicked:
      event = NTP_CUSTOMIZE_SHORTCUT_VISIBILITY_TOGGLE_CLICKED;
      break;
    default:
      NOTREACHED();
  }
  LogEvent(event);
}

void NewTabPageHandler::OnContextualSearchIPHEngaged() {
  auto* browser = webui::GetBrowserWindowInterface(web_contents_);
  if (browser) {
    auto* user_education = BrowserUserEducationInterface::From(browser);
    if (user_education) {
      user_education->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHDesktopRealboxContextualSearchFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    }
  }
}

void NewTabPageHandler::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  OnThemeChanged();
}

void NewTabPageHandler::OnThemeChanged() {
  page_->SetTheme(MakeTheme(web_contents_->GetColorProvider(), theme_provider_,
                            theme_service_, ntp_custom_background_service_,
                            web_contents_));
}

void NewTabPageHandler::OnCustomBackgroundImageUpdated() {
  OnThemeChanged();
}

void NewTabPageHandler::OnChangeInFeatureCurrentlyEnabledState(
    bool is_now_enabled) {
  page_->SetWallpaperSearchButtonVisibility(is_now_enabled);
}

void NewTabPageHandler::OnAuthStateUpdated() {
  UpdateModulesLoadable();
}

void NewTabPageHandler::OnFooterVisibilityUpdated(bool visible) {
  page_->FooterVisibilityUpdated(visible);
}

void NewTabPageHandler::OnBrowserWindowInterfaceChanged() {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    return;
  }

  // TODO(webium): FooterController depends on BrowserView, but WebUIBrowser
  // doesn't have a BrowserView.
  if (webui_browser::IsWebUIBrowserEnabled()) {
    return;
  }

  footer_controller_observation_.Reset();
  auto* browser = webui::GetBrowserWindowInterface(web_contents_);
  if (!browser) {
    // TODO(crbug.com/378475391): NTP should always load into a WebContents
    // owned by a TabModel. Remove this once NTP loading has been restricted to
    // browser tabs only.
    return;
  }

  auto* footer_controller = browser->GetFeatures().new_tab_footer_controller();
  CHECK(footer_controller);
  footer_controller_observation_.Observe(footer_controller);
}

void NewTabPageHandler::MaybeTriggerAutomaticCustomizeChromePromo() {
  feature_promo_helper_->MaybeTriggerAutomaticCustomizeChromePromo(
      web_contents_);
}

void NewTabPageHandler::LogEvent(NTPLoggingEventType event) {
  logger_.LogEvent(event, base::TimeDelta() /* unused */);
}

void NewTabPageHandler::LogEvent(NTPLoggingEventType event,
                                 base::TimeDelta delta) {
  logger_.LogEvent(event, delta);
}

ntp_tiles::TileType NewTabPageHandler::GetTileType() const {
  // TODO(crbug.com/444707872): Update logic to account for multi-select
  // support for shortcuts.
  if (profile_->GetPrefs()->GetBoolean(
          ntp_prefs::kNtpEnterpriseShortcutsVisible)) {
    return ntp_tiles::TileType::kEnterpriseShortcuts;
  }
  return profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpCustomLinksVisible)
             ? ntp_tiles::TileType::kCustomLinks
             : ntp_tiles::TileType::kTopSites;
}

bool NewTabPageHandler::IsShortcutsVisible() const {
  return profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible);
}

void NewTabPageHandler::MaybeLaunchInteractionSurvey(
    std::string_view interaction,
    const std::string& module_id,
    int delay_time_ms) {
  const auto& module_trigger_id =
      GetSurveyTriggerIdForModuleAndInteraction(interaction, module_id);
  if (module_trigger_id.empty()) {
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  CHECK(hats_service);
  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerNtpModules, web_contents_, delay_time_ms, {}, {},
      HatsService::NavigationBehavior::ALLOW_ANY, base::DoNothing(),
      base::DoNothing(), module_trigger_id);
}

void NewTabPageHandler::MaybeShowWebstoreToast() {
  if (profile_->GetPrefs()->GetInteger(prefs::kSeedColorChangeCount) <= 3) {
    page_->ShowWebstoreToast();
  }
}

void NewTabPageHandler::RecordModuleInteraction(const std::string& module_id) {
  DisableModuleAutoRemoval(profile_, module_id);
  IncrementDictPrefKeyCount(prefs::kNtpModulesInteractedCountDict, module_id);
}

void NewTabPageHandler::IncrementDictPrefKeyCount(const std::string& pref_name,
                                                  const std::string& key) {
  const base::DictValue& counts_dict = profile_->GetPrefs()->GetDict(pref_name);
  std::optional<int> count = counts_dict.FindInt(key);
  ScopedDictPrefUpdate update(profile_->GetPrefs(), pref_name);
  update->Set(key,
              count.has_value()
                  ? ((count.value() < INT_MAX) ? count.value() + 1 : INT_MAX)
                  : 1);
}

const std::string& NewTabPageHandler::GetSurveyTriggerIdForModuleAndInteraction(
    std::string_view interaction,
    const std::string& module_id) {
  static const std::string kNoTriggerId;
  DCHECK(kModuleInteractionNames.find(interaction) !=
         kModuleInteractionNames.end());
  const base::DictValue* module_id_trigger_dict =
      interaction_module_id_trigger_dict_.FindDict(interaction);
  if (module_id_trigger_dict) {
    auto* trigger_id = module_id_trigger_dict->FindString(module_id);
    if (trigger_id) {
      return *trigger_id;
    }
  }

  return kNoTriggerId;
}

void NewTabPageHandler::SetModuleHidden(const std::string& module_id,
                                        bool hidden) {
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpHiddenModules);
  base::ListValue& list = update.Get();
  if (hidden) {
    if (!list.contains(module_id)) {
      list.Append(module_id);
    }
  } else {
    list.EraseValue(base::Value(module_id));
  }
}

bool NewTabPageHandler::SyncMicrosoftModulesWithAuth() {
  MicrosoftAuthService::AuthState state =
      microsoft_auth_service_->GetAuthState();

  const std::vector<std::string> auth_dependent_modules(
      ntp_modules::kMicrosoftAuthDependentModuleIds.begin(),
      ntp_modules::kMicrosoftAuthDependentModuleIds.end());
  const std::string auth_id = ntp_modules::kMicrosoftAuthenticationModuleId;
  std::vector<std::string> enabled_modules;
  std::vector<std::string> disabled_modules;
  switch (state) {
    case MicrosoftAuthService::AuthState::kNone:
      break;
    case MicrosoftAuthService::AuthState::kError:
      enabled_modules.push_back(auth_id);
      disabled_modules = auth_dependent_modules;
      break;
    case MicrosoftAuthService::AuthState::kSuccess:
      enabled_modules = auth_dependent_modules;
      disabled_modules.push_back(auth_id);
      break;
    default:
      NOTREACHED();
  }

  for (const auto& module_id : enabled_modules) {
    SetModuleHidden(module_id, false);
  }
  for (const auto& module_id : disabled_modules) {
    SetModuleHidden(module_id, true);
  }

  return state != MicrosoftAuthService::AuthState::kNone;
}

void NewTabPageHandler::TryShowRealboxContextualMenuIPH(
    ui::TrackedElement* element) {
  if (!element || !element->IsA<ui::TrackedElementWebUI>()) {
    return;
  }

  auto* anchor = element->AsA<ui::TrackedElementWebUI>();
  if (anchor->handler()->web_contents() != web_contents_) {
    return;
  }

  // TODO(crbug.com/378475391): NTP should always load into a WebContents.
  auto* browser = webui::GetBrowserWindowInterface(web_contents_);
  if (!browser) {
    return;
  }

  // If a sign-in dialog is being currently displayed, the promo should not be
  // shown to avoid conflict. The sign-in dialog would be shown as soon as the
  // browser is opened, before the promo.
  bool is_signin_modal_dialog_open =
      feature_promo_helper_->IsSigninModalDialogOpen(web_contents_.get());
  if (is_signin_modal_dialog_open) {
    return;
  }

  feature_promo_helper_->MaybeShowFeaturePromo(
      feature_engagement::kIPHDesktopRealboxContextualSearchFeature,
      web_contents_.get());
}

void NewTabPageHandler::ConnectToParentDocument(
    mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
        child_untrusted_document_remote) {
  page_->ConnectToParentDocument(std::move(child_untrusted_document_remote));
}
