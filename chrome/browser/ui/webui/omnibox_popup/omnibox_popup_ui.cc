// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"

#include <atomic>
#include <string_view>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/sanitized_image/sanitized_image_source.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_full_handler.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "chrome/grit/omnibox_popup_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/webui/webui_util.h"

bool OmniboxPopupUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return omnibox::IsWebUIOmniboxFullPopupEnabled() ||
         omnibox::IsWebUIOmniboxPopupEnabled() ||
         base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere) ||
         features::IsWebUILocationBarEnabled();
}

bool OmniboxPopupUIConfig::ShouldCrashOnJavascriptErrorInDevelopmentBuild()
    const {
  return true;
}

OmniboxPopupUI::OmniboxPopupUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui,
                               /*enable_chrome_send=*/true,
                               /*enable_chrome_histograms=*/true),
      profile_(Profile::FromWebUI(web_ui)) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUIOmniboxPopupHost);

  source->AddLocalizedStrings(SearchboxHandler::GetWebUIDataSourceDict(
      Profile::FromWebUI(web_ui),
      {.enable_voice_search = false,
       .enable_lens_search = false,
       .session_allows_drag_and_drop = false}));

  source->AddBoolean("isTopChromeSearchbox", true);
  source->AddBoolean("isTouchUi", ui::TouchUiController::Get()->touch_ui());
  // TODO(b/504670497): Replace this NTP-specific flag with a generic flag.
  // TODO(b/474406096): Replace this NTP-specific flag with a generic flag.
  source->AddBoolean("isFuseboxEnabled", false);
  source->AddBoolean("searchboxDynamicColorScheme",
                     omnibox::kWebUIOmniboxDynamicColorScheme.Get());
  source->AddBoolean("searchboxDynamicAnimation",
                     omnibox::kWebUIOmniboxDynamicAnimation.Get());
  source->AddBoolean("omniboxShowContextButtonSuggestionLabel",
                     omnibox::kContextButtonShowSuggestionLabel.Get());
  source->AddBoolean(
      "omniboxPopupDebugEnabled",
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopupDebug));
  source->AddBoolean("webuiOmniboxPopupSelectionControlEnabled",
                     base::FeatureList::IsEnabled(
                         omnibox::kWebUIOmniboxPopupSelectionControl));
  source->AddBoolean(
      "searchboxMultiline",
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup) &&
          omnibox::kWebUIOmniboxFullPopupMultiline.Get());

  source->AddBoolean("reportMetrics", true);
  source->AddString("charTypedToPaintMetricName",
                    "Omnibox.WebUI.CharTypedToRepaintLatency.ToPaint");
  source->AddString(
      "resultChangedToPaintMetricName",
      "Omnibox.Popup.WebUI.ResultChangedToRepaintLatency.ToPaint");

  source->AddBoolean(
      "caretAnimationEnabled",
      base::FeatureList::IsEnabled(omnibox::kOmniboxAnimatedCaret));
  source->AddBoolean("contextButtonHasBackground",
                     omnibox::kContextButtonHasBackground.Get());
  source->AddBoolean("webuiOmniboxSimplificationEnabled",
                     base::FeatureList::IsEnabled(
                         omnibox::internal::kWebUIOmniboxSimplification));
  source->AddBoolean("hideClassicContextButton",
                     omnibox::kHideClassicContextButton.Get());
  source->AddString("searchboxLayoutMode", "TallBottomContext");
  source->AddBoolean("caretColorAnimationDisabled",
                     base::FeatureList::IsEnabled(
                         omnibox::kWebUIOmniboxDisableCaretColorAnimation));
  source->AddBoolean(
      "energyEffectEnabled",
      base::FeatureList::IsEnabled(omnibox::kEnergyEffectInOmnibox));
  source->AddBoolean(
      "energyEffectAnimationEnabled",
      base::FeatureList::IsEnabled(omnibox::kEnergyEffectInOmnibox));
  source->AddBoolean("contextButtonShapeIsOblong",
                     omnibox::kContextButtonShapeIsOblong.Get());

  int default_resource = IDR_OMNIBOX_POPUP_OMNIBOX_POPUP_HTML;
  if (omnibox::IsWebUIOmniboxFullPopupEnabled()) {
    default_resource = IDR_OMNIBOX_POPUP_OMNIBOX_POPUP_FULL_HTML;
  }
  webui::SetupWebUIDataSource(source, kOmniboxPopupResources, default_resource);
  webui::EnableTrustedTypesCSP(source);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc,
      "media-src blob: data: 'self';");

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("sharingTabs",
                                            IDS_COMPOSE_SHARING_TABS);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFavicon2));
}

OmniboxPopupUI::~OmniboxPopupUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(OmniboxPopupUI)

void OmniboxPopupUI::BindInterface(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory>
        pending_page_handler) {
  if (searchbox_page_factory_receiver_.is_bound()) {
    searchbox_page_factory_receiver_.reset();
  }
  searchbox_page_factory_receiver_.Bind(std::move(pending_page_handler));
}

void OmniboxPopupUI::CreatePageHandler(
    mojo::PendingRemote<searchbox::mojom::Page> page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler) {
  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::GetOrCreateForWebContents(
          web_ui()->GetWebContents())
          ->get_omnibox_controller();
  CHECK(omnibox_controller);

  MetricsReporterService* metrics_reporter_service =
      MetricsReporterService::GetFromWebContents(web_ui()->GetWebContents());
  if (omnibox::ShouldUseWebUIOmniboxFullHandler()) {
    ChromeOmniboxClient* client =
        static_cast<ChromeOmniboxClient*>(omnibox_controller->client());
    CHECK(client);
    omnibox_handler_ = std::make_unique<WebuiOmniboxFullHandler>(
        std::move(pending_page_handler), std::move(page),
        Profile::FromWebUI(web_ui()), web_ui()->GetWebContents(),
        std::make_unique<ChromeOmniboxClient>(
            client->GetLocationBar(), client->browser(), client->profile()));
  } else {
    omnibox_handler_ = std::make_unique<WebuiOmniboxHandler>(
        std::move(pending_page_handler), std::move(page),
        metrics_reporter_service->metrics_reporter(), omnibox_controller,
        web_ui());
  }

  // If presenter delegate is connected, set the delegate in the handler so the
  // handler can notify the delegate when there are embedded permissions prompt
  // changes. Otherwise, once the presenter delegate connects itself, connect
  // the two there instead.
  if (presenter_delegate_) {
    omnibox_handler_->set_delegate(presenter_delegate_);
  }
}

void OmniboxPopupUI::BindInterface(
    mojo::PendingReceiver<omnibox_popup::mojom::PageHandlerFactory> receiver) {
  popup_page_factory_receiver_.reset();
  popup_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxPopupUI::CreatePageHandler(
    mojo::PendingRemote<omnibox_popup::mojom::Page> page,
    mojo::PendingReceiver<omnibox_popup::mojom::PageHandler> receiver) {
  auto* omnibox_controller =
      OmniboxPopupWebContentsHelper::GetOrCreateForWebContents(
          web_ui()->GetWebContents())
          ->get_omnibox_controller();
  CHECK(omnibox_controller);

  popup_handler_ = std::make_unique<OmniboxPopupHandler>(
      std::move(receiver), std::move(page), web_ui()->GetWebContents(),
      omnibox_controller);
  popup_handler_->set_embedder(embedder());
}

void OmniboxPopupUI::SetPresenterDelegate(OmniboxPopupPresenterBase* delegate) {
  presenter_delegate_ = delegate;

  // If the handler is initialized already, set the delegate in the handler so
  // the handler can notify the delegate when there are embedded permissions
  // prompt changes. Otherwise, once the handler is initialized, connect the two
  // there instead.
  if (omnibox_handler_) {
    omnibox_handler_->set_delegate(presenter_delegate_);
  }
}
