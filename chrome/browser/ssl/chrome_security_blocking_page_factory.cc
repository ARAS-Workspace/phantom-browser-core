// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_only_mode_controller_client.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ssl/insecure_form/insecure_form_controller_client.h"
#include "chrome/browser/ssl/ssl_error_controller_client.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_MAC)
#include "base/enterprise_util.h"
#endif

namespace {

enum EnterpriseManaged {
  ENTERPRISE_MANAGED_STATUS_NOT_SET,
  ENTERPRISE_MANAGED_STATUS_TRUE,
  ENTERPRISE_MANAGED_STATUS_FALSE
};

EnterpriseManaged g_is_enterprise_managed_for_testing =
    ENTERPRISE_MANAGED_STATUS_NOT_SET;

std::unique_ptr<security_interstitials::MetricsHelper>
CreateMetricsHelperAndStartRecording(content::WebContents* web_contents,
                                     const GURL& request_url,
                                     const std::string& metric_prefix) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = metric_prefix;
  return std::make_unique<security_interstitials::MetricsHelper>(
      request_url, reporting_info,
      HistoryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          ServiceAccessType::EXPLICIT_ACCESS));
}

}  // namespace

std::unique_ptr<SSLBlockingPage>
ChromeSecurityBlockingPageFactory::CreateSSLPage(
    content::WebContents* web_contents,
    net::Error cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const GURL& support_url) {
  bool overridable = SSLBlockingPage::IsOverridable(options_mask);
  std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper(
      CreateMetricsHelperAndStartRecording(
          web_contents, request_url,
          overridable ? "ssl_overridable" : "ssl_nonoverridable"));

  auto controller_client = std::make_unique<SSLErrorControllerClient>(
      web_contents, ssl_info, cert_error, request_url,
      std::move(metrics_helper));

  std::unique_ptr<SSLBlockingPage> page;

  page = std::make_unique<SSLBlockingPage>(
      web_contents, cert_error, ssl_info, request_url, options_mask,
      time_triggered, support_url, overridable,
      std::move(controller_client));

  return page;
}

std::unique_ptr<LocalSelfSignedBlockingPage>
ChromeSecurityBlockingPageFactory::CreateLocalSelfSignedBlockingPage(
    content::WebContents* web_contents,
    net::Error cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const GURL& support_url) {
  bool overridable = SSLBlockingPage::IsOverridable(options_mask);
  std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper(
      CreateMetricsHelperAndStartRecording(
          web_contents, request_url,
          overridable ? "ssl_overridable" : "ssl_nonoverridable"));

  auto controller_client = std::make_unique<SSLErrorControllerClient>(
      web_contents, ssl_info, cert_error, request_url,
      std::move(metrics_helper));

  auto page = std::make_unique<LocalSelfSignedBlockingPage>(
      web_contents, cert_error, ssl_info, request_url, options_mask,
      time_triggered, support_url, overridable,
      std::move(controller_client));

  return page;
}

std::unique_ptr<BadClockBlockingPage>
ChromeSecurityBlockingPageFactory::CreateBadClockBlockingPage(
    content::WebContents* web_contents,
    net::Error cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    const base::Time& time_triggered,
    ssl_errors::ClockState clock_state) {
  auto page = std::make_unique<BadClockBlockingPage>(
      web_contents, cert_error, ssl_info, request_url, time_triggered,
      clock_state,
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "bad_clock")));

  return page;
}

std::unique_ptr<MITMSoftwareBlockingPage>
ChromeSecurityBlockingPageFactory::CreateMITMSoftwareBlockingPage(
    content::WebContents* web_contents,
    net::Error cert_error,
    const GURL& request_url,
    const net::SSLInfo& ssl_info,
    const std::string& mitm_software_name) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto page = std::make_unique<MITMSoftwareBlockingPage>(
      web_contents, cert_error, request_url,
      ssl_info,
      mitm_software_name, IsEnterpriseManaged(profile),
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "mitm_software")));

  return page;
}

std::unique_ptr<BlockedInterceptionBlockingPage>
ChromeSecurityBlockingPageFactory::CreateBlockedInterceptionBlockingPage(
    content::WebContents* web_contents,
    net::Error cert_error,
    const GURL& request_url,
    const net::SSLInfo& ssl_info) {
  auto page = std::make_unique<BlockedInterceptionBlockingPage>(
      web_contents, cert_error, request_url,
      ssl_info,
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "blocked_interception")));

  return page;
}

std::unique_ptr<security_interstitials::InsecureFormBlockingPage>
ChromeSecurityBlockingPageFactory::CreateInsecureFormBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url) {
  std::unique_ptr<InsecureFormControllerClient> client =
      std::make_unique<InsecureFormControllerClient>(web_contents, request_url);
  auto page =
      std::make_unique<security_interstitials::InsecureFormBlockingPage>(
          web_contents, request_url, std::move(client));
  return page;
}

std::unique_ptr<security_interstitials::HttpsOnlyModeBlockingPage>
ChromeSecurityBlockingPageFactory::CreateHttpsOnlyModeBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    security_interstitials::https_only_mode::HttpInterstitialState
        interstitial_state,
    std::optional<std::string> url_type_param,
    security_interstitials::HttpsOnlyModeBlockingPage::MetricsCallback
        metrics_callback) {
  std::unique_ptr<HttpsOnlyModeControllerClient> client =
      std::make_unique<HttpsOnlyModeControllerClient>(
          web_contents, request_url);

  if (url_type_param) {
    if (*url_type_param == "advanced_protection") {
      interstitial_state.enabled_by_advanced_protection = true;
    } else if (*url_type_param == "site_engagement") {
      interstitial_state.enabled_by_engagement_heuristic = true;
    } else if (*url_type_param == "typically_secure") {
      interstitial_state.enabled_by_typically_secure_browsing = true;
    } else if (*url_type_param == "incognito") {
      interstitial_state.enabled_by_incognito = true;
    }
  } else {
    // HFM interstitial with Site Engagement heuristic is only shown if the
    // feature flag is enabled, so update the relevant flag here.
    interstitial_state.enabled_by_engagement_heuristic =
        interstitial_state.enabled_by_engagement_heuristic &&
        base::FeatureList::IsEnabled(
            features::kHttpsFirstModeV2ForEngagedSites);
  }

  auto page =
      std::make_unique<security_interstitials::HttpsOnlyModeBlockingPage>(
          web_contents, request_url, std::move(client), interstitial_state,
          metrics_callback);
  return page;
}

// static
bool ChromeSecurityBlockingPageFactory::IsEnterpriseManaged(Profile* profile) {
  // Return the value of the testing flag if it's set.
  if (g_is_enterprise_managed_for_testing == ENTERPRISE_MANAGED_STATUS_TRUE) {
    return true;
  }
  if (g_is_enterprise_managed_for_testing == ENTERPRISE_MANAGED_STATUS_FALSE) {
    return false;
  }

#if BUILDFLAG(IS_MAC)
  if (base::IsManagedOrEnterpriseDevice()) {
    return true;
  }
#endif  // BUILDFLAG(IS_MAC)

  if (profile && profile->GetProfilePolicyConnector() &&
      profile->GetProfilePolicyConnector()->IsManaged()) {
    return true;
  }
  return false;
}

// static
void ChromeSecurityBlockingPageFactory::SetEnterpriseManagedForTesting(
    bool enterprise_managed) {
  if (enterprise_managed) {
    g_is_enterprise_managed_for_testing = ENTERPRISE_MANAGED_STATUS_TRUE;
  } else {
    g_is_enterprise_managed_for_testing = ENTERPRISE_MANAGED_STATUS_FALSE;
  }
}
