// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/ui/webui/cr_components/customize_color_scheme_mode/customize_color_scheme_mode_handler.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/theme_color_picker_handler.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar_handler.h"
#include "chrome/browser/ui/webui/theme_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_customize_chrome_resources.h"
#include "chrome/grit/side_panel_customize_chrome_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/webui_util.h"

CustomizeChromeUI::CustomizeChromeUI(content::WebUI* web_ui)
    // Enable chrome.send to notify `ThemeHandler` to observe theme changes.
    : TopChromeWebUIController(web_ui, true),
      profile_(Profile::FromWebUI(web_ui)),
      web_contents_(web_ui->GetWebContents()),
      page_factory_receiver_(this) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUICustomizeChromeSidePanelHost);

  if (!profile_->IsGuestSession()) {
    web_ui->AddMessageHandler(std::make_unique<ThemeHandler>());
  }

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      // Side panel strings.
      {"backButton", IDS_ACCNAME_BACK},
      {"title", IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE},
      // Header strings.
      {"appearanceHeader", IDS_NTP_CUSTOMIZE_APPEARANCE_LABEL},
      {"footerHeader", IDS_NTP_CUSTOMIZE_FOOTER_HEADER},
      {"toolbarHeader", IDS_NTP_CUSTOMIZE_MENU_TOOLBAR_LABEL},
      // Appearance strings.
      {"colorsContainerLabel", IDS_NTP_THEMES_CONTAINER_LABEL},
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"defaultColorName", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"greyDefaultColorName", IDS_NTP_CUSTOMIZE_GREY_DEFAULT_LABEL},
      {"hueSliderTitle", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_TITLE},
      {"hueSliderAriaLabel", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_ARIA_LABEL},
      {"managedColorsTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},
      {"managedColorsBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
      {"resetToClassicChrome",
       IDS_NTP_CUSTOMIZE_CHROME_RESET_TO_CLASSIC_CHROME_LABEL},
      {"updatedToClassicChrome",
       IDS_NTP_CUSTOMIZE_CHROME_RESET_TO_CLASSIC_CHROME_COMPLETE},
      {"newTabPageManagedByA11yLabel",
       IDS_NTP_CUSTOMIZE_CHROME_MANAGED_NEW_TAB_PAGE_ACCESSIBILITY},
      // Footer strings.
      {"showFooterToggleTitle", IDS_NTP_CUSTOMIZE_SHOW_FOOTER_LABEL},
      // Required by <managed-dialog>.
      {"controlledSettingPolicy", IDS_CONTROLLED_SETTING_POLICY},
      {"close", IDS_NTP_CLOSE},
      {"ok", IDS_OK},
      // CustomizeColorSchemeMode strings.
      {"colorSchemeModeLabel",
       IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_GROUP_LABEL},
      {"lightMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_LIGHT_LABEL},
      {"darkMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_DARK_LABEL},
      {"systemMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_SYSTEM_LABEL},
      // Customize Toolbar strings.
      {"enterpriseEnabledLabel",
       IDS_NTP_CUSTOMIZE_TOOLBAR_ENTERPRISE_ENABLED_LABEL},
      {"toolbarButtonA11yLabel", IDS_NTP_CUSTOMIZE_TOOLBAR_BUTTON_A11Y_LABEL},
      {"chooseToolbarIconsLabel", IDS_NTP_CUSTOMIZE_TOOLBAR_CHOOSE_ICONS_LABEL},
      {"resetToDefaultButtonLabel",
       IDS_NTP_CUSTOMIZE_TOOLBAR_RESET_TO_DEFAULT_BUTTON_LABEL},
      {"resetToDefaultButtonAnnouncement",
       IDS_NTP_CUSTOMIZE_TOOLBAR_RESET_TO_DEFAULT_ANNOUNCEMENT},
      {"reorderTipLabel", IDS_NTP_CUSTOMIZE_TOOLBAR_REORDER_TIP_LABEL},
      {"managedA11yLabel", IDS_MANAGED},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  source->AddBoolean("footerEnabled",
                     base::FeatureList::IsEnabled(ntp_features::kNtpFooter));

  webui::SetupWebUIDataSource(
      source, kSidePanelCustomizeChromeResources,
      IDR_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_HTML);
  source->AddResourcePaths(kSidePanelSharedResources);

  content::URLDataSource::Add(profile_,
                              std::make_unique<ThemeSource>(profile_));
}

CustomizeChromeUI::~CustomizeChromeUI() = default;

void CustomizeChromeUI::ScrollToSection(CustomizeChromeSection section) {
  if (customize_chrome_page_handler_) {
    customize_chrome_page_handler_->ScrollToSection(section);
  } else {
    section_ = section;
  }
}

void CustomizeChromeUI::AttachedTabStateUpdated(const GURL& url) {
  if (customize_chrome_page_handler_) {
    customize_chrome_page_handler_->AttachedTabStateUpdated(url);
  } else {
    source_tab_url_ = url;
  }
}

base::WeakPtr<CustomizeChromeUI> CustomizeChromeUI::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WEB_UI_CONTROLLER_TYPE_IMPL(CustomizeChromeUI)

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandlerFactory>
        receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }
  page_factory_receiver_.Bind(std::move(receiver));
}

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<
        side_panel::customize_chrome::mojom::CustomizeToolbarHandlerFactory>
        receiver) {
  if (customize_toolbar_handler_factory_receiver_.is_bound()) {
    customize_toolbar_handler_factory_receiver_.reset();
  }
  customize_toolbar_handler_factory_receiver_.Bind(std::move(receiver));
}

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<customize_color_scheme_mode::mojom::
                              CustomizeColorSchemeModeHandlerFactory>
        pending_receiver) {
  if (customize_color_scheme_mode_handler_factory_receiver_.is_bound()) {
    customize_color_scheme_mode_handler_factory_receiver_.reset();
  }
  customize_color_scheme_mode_handler_factory_receiver_.Bind(
      std::move(pending_receiver));
}

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<
        theme_color_picker::mojom::ThemeColorPickerHandlerFactory>
        pending_receiver) {
  if (theme_color_picker_handler_factory_receiver_.is_bound()) {
    theme_color_picker_handler_factory_receiver_.reset();
  }
  theme_color_picker_handler_factory_receiver_.Bind(
      std::move(pending_receiver));
}

void CustomizeChromeUI::CreatePageHandler(
    mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
    mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());
  customize_chrome_page_handler_ = std::make_unique<CustomizeChromePageHandler>(
      std::move(pending_page_handler), std::move(pending_page),
      NtpCustomBackgroundServiceFactory::GetForProfile(profile_),
      web_contents_);
  if (section_.has_value()) {
    customize_chrome_page_handler_->ScrollToSection(*section_);
    section_.reset();
  }
  if (!source_tab_url_.is_empty()) {
    customize_chrome_page_handler_->AttachedTabStateUpdated(source_tab_url_);
  }
}

void CustomizeChromeUI::CreateCustomizeColorSchemeModeHandler(
    mojo::PendingRemote<
        customize_color_scheme_mode::mojom::CustomizeColorSchemeModeClient>
        client,
    mojo::PendingReceiver<
        customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandler>
        handler) {
  customize_color_scheme_mode_handler_ =
      std::make_unique<CustomizeColorSchemeModeHandler>(
          std::move(client), std::move(handler), profile_);
}

void CustomizeChromeUI::CreateThemeColorPickerHandler(
    mojo::PendingRemote<theme_color_picker::mojom::ThemeColorPickerClient>
        client,
    mojo::PendingReceiver<theme_color_picker::mojom::ThemeColorPickerHandler>
        handler) {
  theme_color_picker_handler_ = std::make_unique<ThemeColorPickerHandler>(
      std::move(handler), std::move(client),
      NtpCustomBackgroundServiceFactory::GetForProfile(profile_),
      web_contents_);
}

void CustomizeChromeUI::CreateCustomizeToolbarHandler(
    mojo::PendingRemote<
        side_panel::customize_chrome::mojom::CustomizeToolbarClient> client,
    mojo::PendingReceiver<
        side_panel::customize_chrome::mojom::CustomizeToolbarHandler> handler) {
  if (customize_toolbar_handler_) {
    mojo::ReportBadMessage("Only allowed to create one Mojo pipe per WebUI.");
    return;
  }

  customize_toolbar_handler_ = std::make_unique<CustomizeToolbarHandler>(
      std::move(handler), std::move(client), web_contents_);
}
