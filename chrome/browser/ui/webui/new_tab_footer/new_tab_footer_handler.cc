// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/webui/new_tab_footer/footer_context_menu.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"
#include "chrome/browser/ui/webui/util/webui_util_desktop.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/url_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/url_constants.h"

NewTabFooterHandler::NewTabFooterHandler(
    mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>
        pending_handler,
    mojo::PendingRemote<new_tab_footer::mojom::NewTabFooterDocument>
        pending_document,
    base::WeakPtr<TopChromeWebUIController::Embedder> embedder,
    NtpCustomBackgroundService* ntp_custom_background_service,
    content::WebContents* web_contents)
    : embedder_(embedder),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      ntp_custom_background_service_(ntp_custom_background_service),
      theme_provider_(&ThemeService::GetThemeProviderForProfile(profile_)),
      feature_promo_helper_{std::make_unique<NewTabPageFeaturePromoHelper>()},
      document_(std::move(pending_document)),
      handler_{this, std::move(pending_handler)} {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(profile_));
  if (ntp_custom_background_service_) {
    ntp_custom_background_service_observation_.Observe(
        ntp_custom_background_service_);
  }
  profile_pref_change_registrar_.Init(profile_->GetPrefs());
  profile_pref_change_registrar_.Add(
      prefs::kNTPFooterExtensionAttributionEnabled,
      base::BindRepeating(&NewTabFooterHandler::UpdateNtpExtensionName,
                          base::Unretained(this)));
}

NewTabFooterHandler::~NewTabFooterHandler() = default;

void NewTabFooterHandler::UpdateNtpExtensionName() {
  std::string id;
  std::string name;
  bool attribution_enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled);
  if (attribution_enabled) {
    if (const extensions::Extension* ntp_extension =
            extensions::GetExtensionOverridingNewTabPage(profile_)) {
      id = ntp_extension->id();
      name = ntp_extension->name();
    }
  }
  curr_ntp_extension_id_ = id;
  document_->SetNtpExtensionName(std::move(name));
  }

  void NewTabFooterHandler::UpdateBackgroundAttribution() {
    OnCustomBackgroundImageUpdated();
  }

  void NewTabFooterHandler::SetThemeProviderForTesting(
      ui::ThemeProvider* theme_provider) {
    theme_provider_ = theme_provider;
  }

void NewTabFooterHandler::OpenExtensionOptionsPageWithFallback() {
  GURL options_url = GURL(chrome::kChromeUIExtensionsURL);
  if (!curr_ntp_extension_id_.empty()) {
    options_url = net::AppendOrReplaceQueryParameter(options_url, "id",
                                                     curr_ntp_extension_id_);
  }
  OpenUrlInCurrentTabInternal(options_url);
}

void NewTabFooterHandler::ShowContextMenu(const gfx::Point& point) {
  if (!embedder_) {
    return;
  }

  auto* browser = webui::GetBrowserWindowInterface(web_contents_);
  if (browser) {
    embedder_->ShowContextMenu(point,
                               std::make_unique<FooterContextMenu>(browser));
  }
}

void NewTabFooterHandler::NotifyCustomizationButtonVisible() {
  feature_promo_helper_->MaybeTriggerAutomaticCustomizeChromePromo(
      web_contents_);
}

void NewTabFooterHandler::OpenUrlInCurrentTab(const GURL& url) {
  // Mojo entry: renderer-supplied URLs become browser-initiated navigations,
  // so only https:// is allowed; anything else is treated as a bad message.
  if (!url.SchemeIs(url::kHttpsScheme)) {
    mojo::ReportBadMessage("OpenUrlInCurrentTab: scheme must be https");
    return;
  }
  OpenUrlInCurrentTabInternal(url);
}

void NewTabFooterHandler::OpenUrlInCurrentTabInternal(const GURL& url) {
  auto* browser_window = webui::GetBrowserWindowInterface(web_contents_);
  if (!browser_window || !url.is_valid()) {
    return;
  }

  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
}

void NewTabFooterHandler::OnExtensionReady(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  UpdateNtpExtensionName();
}

void NewTabFooterHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  UpdateNtpExtensionName();
}

void NewTabFooterHandler::UpdateAttachedTabState() {
  AttachedTabStateUpdated(last_source_url_);
}

void NewTabFooterHandler::AttachedTabStateUpdated(const GURL& url) {
  last_source_url_ = url;
  new_tab_footer::mojom::NewTabPageType ntp_type =
      new_tab_footer::mojom::NewTabPageType::kOther;
  if (NewTabPageUI::IsNewTabPageOrigin(url)) {
    ntp_type = new_tab_footer::mojom::NewTabPageType::kFirstPartyWebUI;
  } else if (ntp_footer::IsExtensionNtp(url, profile_)) {
    ntp_type = new_tab_footer::mojom::NewTabPageType::kExtension;
  }

  bool can_customize_chrome = CustomizeChromePageHandler::IsSupported(
      NtpCustomBackgroundServiceFactory::GetForProfile(profile_), profile_);

  document_->AttachedTabStateUpdated(ntp_type, can_customize_chrome);
}

void NewTabFooterHandler::OnCustomBackgroundImageUpdated() {
  const bool theme_has_custom_image =
      theme_provider_->HasCustomImage(IDR_THEME_NTP_BACKGROUND);
  if (theme_has_custom_image) {
    document_->SetBackgroundAttribution(nullptr);
    return;
  }

  auto custom_background =
      ntp_custom_background_service_
          ? ntp_custom_background_service_->GetCustomBackground()
          : std::nullopt;
  if (!custom_background.has_value() ||
      custom_background->custom_background_attribution_line_1.empty()) {
    document_->SetBackgroundAttribution(nullptr);
    return;
  }

  auto attribution = new_tab_footer::mojom::BackgroundAttribution::New();
  attribution->name =
      custom_background->custom_background_attribution_line_2.empty()
          ? custom_background->custom_background_attribution_line_1
          : l10n_util::GetStringFUTF8(
                IDS_NEW_TAB_FOOTER_BACKGROUND_ATTRIBUTION_TEXT,
                base::UTF8ToUTF16(
                    custom_background->custom_background_attribution_line_1),
                base::UTF8ToUTF16(
                    custom_background->custom_background_attribution_line_2));

  attribution->url =
      custom_background->custom_background_attribution_action_url;
  document_->SetBackgroundAttribution(std::move(attribution));
}
