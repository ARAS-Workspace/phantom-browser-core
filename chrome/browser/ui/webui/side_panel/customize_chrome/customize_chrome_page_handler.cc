// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"

#include <utility>

#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_utils.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"

namespace {

void OpenWebPage(Profile* profile, const GURL& url) {
  NavigateParams navigate_params(profile, url, ui::PAGE_TRANSITION_LINK);
  navigate_params.window_action = NavigateParams::WindowAction::kShowWindow;
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&navigate_params);
}

}  // namespace

// static
bool CustomizeChromePageHandler::IsSupported(
    NtpCustomBackgroundService* ntp_custom_background_service,
    Profile* profile) {
  if (!ntp_custom_background_service) {
    return false;
  }

  if (!ThemeServiceFactory::GetForProfile(profile)) {
    return false;
  }

  return true;
}

CustomizeChromePageHandler::CustomizeChromePageHandler(
    mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
        pending_page_handler,
    mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
    NtpCustomBackgroundService* ntp_custom_background_service,
    content::WebContents* web_contents,
    std::optional<base::RepeatingCallback<void(const GURL&)>> open_url_callback)
    : ntp_custom_background_service_(ntp_custom_background_service),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      theme_service_(ThemeServiceFactory::GetForProfile(profile_)),
      browser_window_changed_subscription_(
          webui::RegisterBrowserWindowInterfaceChanged(
              web_contents_,
              base::BindRepeating(
                  &CustomizeChromePageHandler::OnBrowserWindowInterfaceChanged,
                  base::Unretained(this)))),
      page_(std::move(pending_page)),
      receiver_(this, std::move(pending_page_handler)),
      open_url_callback_(open_url_callback.has_value()
                             ? open_url_callback.value()
                             : base::BindRepeating(&OpenWebPage, profile_)) {
  CHECK(IsSupported(ntp_custom_background_service_, profile_));

  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  theme_service_observation_.Observe(theme_service_);

  OnBrowserWindowInterfaceChanged();

  ntp_custom_background_service_observation_.Observe(
      ntp_custom_background_service_.get());
}

CustomizeChromePageHandler::~CustomizeChromePageHandler() = default;

void CustomizeChromePageHandler::ScrollToSection(
    CustomizeChromeSection section) {
  last_requested_section_ = section;
  if (section == CustomizeChromeSection::kUnspecified) {
    // Cannot scroll to unspecified section.
    return;
  }
  page_->ScrollToSection(section);
}

void CustomizeChromePageHandler::AttachedTabStateUpdated(const GURL& url) {
  last_source_url_ = url;
  page_->AttachedTabStateUpdated(GetNewTabPageType(url));
}

void CustomizeChromePageHandler::SetDefaultColor() {
  theme_service_->UseDefaultTheme();
}

void CustomizeChromePageHandler::RemoveBackgroundImage() {
  if (ntp_custom_background_service_) {
    ntp_custom_background_service_->ResetCustomBackgroundInfo();
    customize_chrome::MaybeDisableExtensionOverridingNtp(profile_);
  }
}

void CustomizeChromePageHandler::UpdateTheme() {
  auto theme = side_panel::mojom::Theme::New();
  auto custom_background =
      ntp_custom_background_service_
          ? ntp_custom_background_service_->GetCustomBackground()
          : std::nullopt;
  theme->has_background_image = custom_background.has_value();
  theme->background_managed_by_policy =
      ntp_custom_background_service_->IsCustomBackgroundDisabledByPolicy();
  if (theme_service_->UsingExtensionTheme()) {
    const extensions::Extension* theme_extension =
        extensions::ExtensionRegistry::Get(profile_)
            ->enabled_extensions()
            .GetByID(theme_service_->GetThemeID());
    theme->has_third_party_theme = theme_extension != nullptr;
  }
  page_->SetTheme(std::move(theme));
}

void CustomizeChromePageHandler::OpenNtpManagedByPage() {
  const extensions::Extension* extension_managing_ntp =
      extensions::GetExtensionOverridingNewTabPage(profile_);
  if (extension_managing_ntp) {
    open_url_callback_.Run(
        net::AppendOrReplaceQueryParameter(GURL(chrome::kChromeUIExtensionsURL),
                                           "id", extension_managing_ntp->id()));
  }
}

void CustomizeChromePageHandler::OnBrowserWindowInterfaceChanged() {
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

void CustomizeChromePageHandler::SetFooterVisible(bool visible) {
  profile_->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, visible);
}

void CustomizeChromePageHandler::UpdateFooterSettings() {
  page_->SetFooterSettings(
      profile_->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible),
      profile_->GetPrefs()->GetBoolean(
          prefs::kNTPFooterExtensionAttributionEnabled));
}

void CustomizeChromePageHandler::UpdateScrollToSection() {
  ScrollToSection(last_requested_section_);
}

void CustomizeChromePageHandler::UpdateAttachedTabState() {
  AttachedTabStateUpdated(last_source_url_);
}

void CustomizeChromePageHandler::UpdateNtpManagedByName() {
  std::string name;
  std::string description;

  const extensions::Extension* extension_managing_ntp =
      extensions::GetExtensionOverridingNewTabPage(profile_);
  if (extension_managing_ntp) {
    name = extension_managing_ntp->short_name();
    description = l10n_util::GetStringUTF8(IDS_NTP_MANAGED_BY_EXTENSION);
  }

  page_->NtpManagedByNameUpdated(name, description);
}

void CustomizeChromePageHandler::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  UpdateTheme();
}

void CustomizeChromePageHandler::OnThemeChanged() {
  UpdateTheme();
}

void CustomizeChromePageHandler::OnCustomBackgroundImageUpdated() {
  OnThemeChanged();
}

void CustomizeChromePageHandler::OnFooterVisibilityUpdated(bool visible) {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    return;
  }
  UpdateFooterSettings();
}

side_panel::mojom::NewTabPageType CustomizeChromePageHandler::GetNewTabPageType(
    const GURL& url) {
  if (ntp_footer::IsExtensionNtp(url, profile_)) {
    return side_panel::mojom::NewTabPageType::kExtension;
  } else if (NewTabPageThirdPartyUI::IsNewTabPageOrigin(url)) {
    return side_panel::mojom::NewTabPageType::kThirdPartyWebUI;
  }

  return side_panel::mojom::NewTabPageType::kNone;
}
