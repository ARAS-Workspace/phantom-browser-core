// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/logging/logs_page_handler.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_page_handler.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/grit/omnibox_resources.h"
#include "chrome/grit/omnibox_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

#include "chrome/browser/ui/webui/theme_source.h"

bool OmniboxUIConfig::SupportsInProcessResourceLoadingV2() const {
  return true;
}

bool OmniboxUIConfig::ShouldCrashOnJavascriptErrorInDevelopmentBuild() const {
  return true;
}

OmniboxUI::OmniboxUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  // Set up the chrome://omnibox/ source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIOmniboxHost);

  webui::SetupWebUIDataSource(source, kOmniboxResources,
                              IDR_OMNIBOX_OMNIBOX_HTML);

  content::URLDataSource::Add(
      Profile::FromWebUI(web_ui),
      std::make_unique<ThemeSource>(Profile::FromWebUI(web_ui)));

  // Expose version information to client because it is useful in output.
  VersionUI::AddVersionDetailStrings(source);

  source->AddResourcePath("logging", IDR_OMNIBOX_LOGGING_LOGS_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(OmniboxUI)

OmniboxUI::~OmniboxUI() = default;

void OmniboxUI::BindInterface(
    mojo::PendingReceiver<mojom::OmniboxPageHandler> receiver) {
  omnibox_handler_ = std::make_unique<OmniboxPageHandler>(
      Profile::FromWebUI(web_ui()), std::move(receiver));
}

void OmniboxUI::BindInterface(
    mojo::PendingReceiver<omnibox::logging::mojom::PageHandlerFactory>
        receiver) {
  if (logs_factory_receiver_.is_bound()) {
    logs_factory_receiver_.reset();
  }
  logs_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxUI::CreatePageHandler(
    mojo::PendingRemote<omnibox::logging::mojom::Page> page,
    mojo::PendingReceiver<omnibox::logging::mojom::PageHandler> handler) {
  logs_page_handler_ = std::make_unique<omnibox::logging::LogsPageHandler>(
      std::move(handler), std::move(page));
}
