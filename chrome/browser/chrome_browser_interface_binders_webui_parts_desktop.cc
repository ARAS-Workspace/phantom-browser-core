// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/chrome_browser_interface_binders_webui_parts.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/history/history_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_coordinator.h"
#include "chrome/browser/ui/webui/color_pipeline_internals/color_pipeline_internals_ui.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons.mojom.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing.mojom.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/infobar_internals/infobar_internals.mojom.h"
#include "chrome/browser/ui/webui/infobar_internals/infobar_internals_ui.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/omnibox/logging/logs.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/debug/omnibox_everywhere_debug.mojom.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/comments/comments.mojom.h"
#include "chrome/browser/ui/webui/side_panel/comments/comments_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/browser/ui/webui/side_panel/history/history_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/history_clusters/history_clusters_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_ui.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals.mojom.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals_ui.h"
#include "chrome/browser/ui/webui/webui_gallery/webui_gallery_ui.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"
#include "chrome/common/chrome_features.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/data_sharing/public/features.h"
#include "components/guest_contents/common/guest_contents.mojom.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom.h"
#include "ui/webui/resources/cr_components/help_bubble/custom_help_bubble.mojom.h"
#include "ui/webui/resources/cr_components/history/foreign_sessions.mojom.h"
#include "ui/webui/resources/cr_components/history/history.mojom.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"
#include "ui/webui/resources/cr_components/theme_color_picker/theme_color_picker.mojom.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal.mojom.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal_ui.h"

namespace chrome::internal {

using content::RegisterWebUIControllerInterfaceBinder;

namespace {

void BindMetricsReporterService(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents) {
    return;
  }

  // MetricsReporterService is only intended for use by WebUI.
  if (!frame_host->GetWebUI()) {
    return;
  }

  MetricsReporterService* service =
      MetricsReporterService::GetFromWebContents(web_contents);
  service->BindReceiver(std::move(receiver));
}

}  // namespace

void PopulateChromeWebUIFrameBindersPartsDesktop(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
  RegisterWebUIControllerInterfaceBinder<downloads::mojom::PageHandlerFactory,
                                         DownloadsUI>(map);

  if (HistorySidePanelCoordinator::IsSupported()) {
    RegisterWebUIControllerInterfaceBinder<history::mojom::PageHandler,
                                           HistorySidePanelUI, HistoryUI>(map);
  } else {
    RegisterWebUIControllerInterfaceBinder<history::mojom::PageHandler,
                                           HistoryUI>(map);
  }
  if (TabsFromOtherDevicesSidePanelCoordinator::IsSupported(
          Profile::FromBrowserContext(
              render_frame_host->GetBrowserContext()))) {
    RegisterWebUIControllerInterfaceBinder<
        history::mojom::ForeignSessionPageHandlerFactory,
        TabsFromOtherDevicesSidePanelUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      infobar_internals::mojom::PageHandlerFactory, InfoBarInternalsUI>(map);

  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(
          render_frame_host->GetProcess()->GetBrowserContext());
  if (history_clusters_service &&
      history_clusters_service->is_journeys_feature_flag_enabled()) {
    if (HistorySidePanelCoordinator::IsSupported()) {
      RegisterWebUIControllerInterfaceBinder<
          history_clusters::mojom::PageHandlerFactory, HistoryUI,
          HistorySidePanelUI>(map);
    } else {
      RegisterWebUIControllerInterfaceBinder<
          history_clusters::mojom::PageHandlerFactory, HistoryUI,
          HistoryClustersSidePanelUI>(map);
    }
  }

  if (HistorySidePanelCoordinator::IsSupported()) {
    RegisterWebUIControllerInterfaceBinder<
        page_image_service::mojom::PageImageServiceHandler, HistoryUI,
        HistorySidePanelUI, BookmarksSidePanelUI>(map);
  } else {
    RegisterWebUIControllerInterfaceBinder<
        page_image_service::mojom::PageImageServiceHandler, HistoryUI,
        HistoryClustersSidePanelUI, BookmarksSidePanelUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<searchbox::mojom::PageHandlerFactory,
                                         OmniboxPopupUI, OmniboxEverywhereUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      omnibox_everywhere_debug::mojom::PageHandlerFactory, OmniboxEverywhereUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      customize_color_scheme_mode::mojom::
          CustomizeColorSchemeModeHandlerFactory,
      CustomizeChromeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      theme_color_picker::mojom::ThemeColorPickerHandlerFactory,
      CustomizeChromeUI
      ,
      ProfileCustomizationUI
      >(map);

  RegisterWebUIControllerInterfaceBinder<
      help_bubble::mojom::HelpBubbleHandlerFactory, UserEducationInternalsUI,
      ProfilePickerUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      side_panel::mojom::BookmarksPageHandlerFactory, BookmarksSidePanelUI>(
      map);
  RegisterWebUIControllerInterfaceBinder<comments::mojom::PageHandlerFactory,
                                         CommentsSidePanelUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      side_panel::mojom::CustomizeChromePageHandlerFactory, CustomizeChromeUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      side_panel::customize_chrome::mojom::CustomizeToolbarHandlerFactory,
      CustomizeChromeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ::mojom::user_education_internals::UserEducationInternalsPageHandler,
      UserEducationInternalsUI>(map);

  // TODO(crbug.com/398926117): Create a generic mechanism for these interfaces.
  map->Add<metrics_reporter::mojom::PageMetricsHost>(
      &BindMetricsReporterService);

  RegisterWebUIControllerInterfaceBinder<
      guest_contents::mojom::GuestContentsHost, WebUIBrowserUI>(map);

  // Registering bindings for all WebUIControllers, even if only one of the
  // features is enabled, as it is too cumbersome and not scalable to account
  // for all combinations of feature flags here.
  // TODO(crbug.com/452983498): This should be fixed by following the
  // registry.ForWebUI() pattern used in
  // PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsDesktop below which
  // eliminates the need to account for feature flag combinations.

  auto prompt_surface = default_browser::GetDefaultBrowserPromptSurface();

  if (prompt_surface == default_browser::DefaultBrowserPromptSurface::
                            kModalDialogWithoutSettingsIllustration ||
      prompt_surface == default_browser::DefaultBrowserPromptSurface::
                            kModalDialogWithSettingsIllustration) {
    RegisterWebUIControllerInterfaceBinder<
        default_browser_modal::mojom::PageHandlerFactory,
        DefaultBrowserModalUI>(map);
  }

}

void PopulateChromeWebUIFrameInterfaceBrokersTrustedPartsDesktop(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  // Note: The MetricsReporterService & ColorChangeListener are available to all
  // WebUIs in the registry
  registry.AddGlobal<metrics_reporter::mojom::PageMetricsHost>(
      base::BindRepeating(&BindMetricsReporterService));

  registry.ForWebUI<TabSearchUI>().Add<tab_search::mojom::PageHandlerFactory>();

  if (base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    registry.ForWebUI<NewTabFooterUI>()
        .Add<customize_buttons::mojom::CustomizeButtonsHandlerFactory>()
        .Add<new_tab_footer::mojom::NewTabFooterHandlerFactory>();
  }

  registry.ForWebUI<NewTabPageThirdPartyUI>()
      .Add<most_visited::mojom::MostVisitedPageHandlerFactory>()
      .Add<new_tab_page_third_party::mojom::PageHandlerFactory>();

  registry
      .ForWebUI<settings::SettingsUI>()
      .Add<theme_color_picker::mojom::ThemeColorPickerHandlerFactory>()
      .Add<signin::mojom::SigninPageHandlerFactory>()
      .Add<customize_color_scheme_mode::mojom::
               CustomizeColorSchemeModeHandlerFactory>()
      .Add<help_bubble::mojom::HelpBubbleHandlerFactory>();

  if (webui_browser::IsWebUIBrowserEnabled()) {
    registry.ForWebUI<WebUIBrowserUI>()
        .Add<webui_browser::mojom::PageHandlerFactory>()
        .Add<bookmark_bar::mojom::PageHandlerFactory>()
        .Add<bookmarks_api::mojom::BookmarksService>()
        .Add<extensions_bar::mojom::PageHandlerFactory>()
        .Add<searchbox::mojom::PageHandlerFactory>()
        .Add<tabs_api::mojom::TabDragService>()
        .Add<tabs_api::mojom::TabStripService>()
        .Add<tabs_api::mojom::TabStripExperimentService>()
        .Add<tabs_api::mojom::TabStripUIController>();
  }

  if (features::IsWebUIToolbarEnabled() ||
      base::FeatureList::IsEnabled(
          features::kWebUIToolbarProcessOverheadExperiment)) {
    registry.ForWebUI<WebUIToolbarUI>()
        .Add<browser_controls_api::mojom::BrowserControlsService>()
        .Add<toolbar_ui_api::mojom::ToolbarUIService>()
        .Add<help_bubble::mojom::HelpBubbleHandlerFactory>();
  }

  // TODO(crbug.com/452983498): Migrate all remaining
  // RegisterWebUIControllerInterfaceBinder calls to registry.ForWebUI().Add()
  // calls.
}

void PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsDesktop(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  if (data_sharing::features::IsDataSharingFunctionalityEnabled()) {
    registry.ForWebUI<DataSharingUI>()
        .Add<data_sharing::mojom::PageHandlerFactory>();
  }
}

}  // namespace chrome::internal
