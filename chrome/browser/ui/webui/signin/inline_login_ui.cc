// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_ui.h"

#include <memory>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/gaia_auth_host_resources.h"
#include "chrome/grit/gaia_auth_host_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/inline_login_resources.h"
#include "chrome/grit/inline_login_resources_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"

#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"

namespace {

void CreateAndAddWebUIDataSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIChromeSigninHost);

  source->AddResourcePaths(kInlineLoginResources);
  webui::SetupWebUIDataSource(source, kGaiaAuthHostResources,
                              IDR_INLINE_LOGIN_INLINE_LOGIN_HTML);
  // TODO(crbug.com/40250068): Remove this when saml_password_attributes.js is
  // made TrustedTypes compliant.
  source->DisableTrustedTypesCSP();
  // Necessary since this UI sends XML Http requests.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc, "connect-src *;");

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"accessibleCloseButtonLabel", IDS_SIGNIN_ACCESSIBLE_CLOSE_BUTTON},
      {"accessibleBackButtonLabel", IDS_SIGNIN_ACCESSIBLE_BACK_BUTTON},
      {"title", IDS_CHROME_SIGNIN_TITLE},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

}

// Returns whether `url` can be displayed in a chrome://chrome-signin web
// contents, depending on the signin reason that is encoded in the url.
// For testing purposes on Windows, loading `chrome://chrome-signin/?reason=6`
// (mapping `signin_metrics::Reason::kFetchLstOnly`) would allow to load the
// page in a tab. On ChromeOS, any reason (or no reason) would work.
// If the reason is not valid, the page should not be loaded.
bool IsValidChromeSigninReason(const GURL& url) {
  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(url);

  switch (reason) {
    case signin_metrics::Reason::kFetchLstOnly:
      // Used by the Google Credential Provider for Windows.
      return true;
    case signin_metrics::Reason::kReauthentication:
    case signin_metrics::Reason::kSigninPrimaryAccount:
    case signin_metrics::Reason::kAddSecondaryAccount:
    case signin_metrics::Reason::kUnknownReason:
      // This can happen if the page is loaded directly into a tab, which may
      // not contain the right reason parameter. In this case, do not load the
      // page instead of crashing. Check crbug.com/479741617.
      return false;
  }
}

}  // namespace

InlineLoginUI::InlineLoginUI(content::WebUI* web_ui) : WebDialogUI(web_ui) {
  if (!IsValidChromeSigninReason(web_ui->GetWebContents()->GetVisibleURL())) {
    // Do not load the page is the reason is not valid.
    return;
  }

  // Always instantiate the WebUIDataSource so that tests pulling deps from
  // from chrome://chrome-signin/gaia_auth_host/ can work.
  Profile* profile = Profile::FromWebUI(web_ui);
  CreateAndAddWebUIDataSource(profile);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  web_ui->AddMessageHandler(std::make_unique<InlineLoginHandlerImpl>());

  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  content::WebContents* contents = web_ui->GetWebContents();
  extensions::TabHelper::CreateForWebContents(contents);
  // Ensure that the login UI has a tab ID, which will allow the GAIA auth
  // extension's background script to tell it apart from iframes injected by
  // other extensions.
  CreateSessionServiceTabHelper(contents);
}

InlineLoginUI::~InlineLoginUI() = default;
