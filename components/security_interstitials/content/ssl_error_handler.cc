// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/ssl_error_handler.h"

#include <stdint.h>

#include <memory>
#include <unordered_set>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/blocked_interception_blocking_page.h"
#include "components/security_interstitials/content/mitm_software_blocking_page.h"
#include "components/security_interstitials/content/security_blocking_page_factory.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_error_assistant.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "components/ssl_errors/error_classification.h"
#include "components/ssl_errors/error_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

BASE_FEATURE(kMITMSoftwareInterstitial, base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

BASE_FEATURE(kSSLCommonNameMismatchHandling, base::FEATURE_ENABLED_BY_DEFAULT);

// Default delay in milliseconds before displaying the SSL interstitial.
// This can be changed in tests.
// - If there is a name mismatch and a suggested URL available result arrives
//   during this time, the user is redirected to the suggester URL.
// - Otherwise, an SSL interstitial is displayed.
const int64_t kInterstitialDelayInMilliseconds = 3000;

const char kHistogram[] = "interstitial.ssl_error_handler";

// Adds a message to console after navigation commits and then, deletes itself.
// Also deletes itself if the navigation is stopped.
class CommonNameMismatchRedirectObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<CommonNameMismatchRedirectObserver> {
 public:
  CommonNameMismatchRedirectObserver(
      const CommonNameMismatchRedirectObserver&) = delete;
  CommonNameMismatchRedirectObserver& operator=(
      const CommonNameMismatchRedirectObserver&) = delete;

  ~CommonNameMismatchRedirectObserver() override = default;

  static void AddToConsoleAfterNavigation(
      content::WebContents* web_contents,
      const std::string& request_url_hostname,
      const std::string& suggested_url_hostname) {
    web_contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new CommonNameMismatchRedirectObserver(
            web_contents, request_url_hostname, suggested_url_hostname)));
  }

 private:
  friend class content::WebContentsUserData<CommonNameMismatchRedirectObserver>;
  CommonNameMismatchRedirectObserver(content::WebContents* web_contents,
                                     const std::string& request_url_hostname,
                                     const std::string& suggested_url_hostname)
      : WebContentsObserver(web_contents),
        content::WebContentsUserData<CommonNameMismatchRedirectObserver>(
            *web_contents),
        request_url_hostname_(request_url_hostname),
        suggested_url_hostname_(suggested_url_hostname) {}

  // WebContentsObserver:
  void NavigationStopped() override {
    // Deletes |this|.
    GetWebContents().RemoveUserData(UserDataKey());
  }

  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& /* load_details */) override {
    GetWebContents().GetPrimaryMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kInfo,
        base::StringPrintf(
            "Redirecting navigation %s -> %s because the server presented a "
            "certificate valid for %s but not for %s. To disable such "
            "redirects launch Chrome with the following flag: "
            "--disable-features=SSLCommonNameMismatchHandling",
            request_url_hostname_.c_str(), suggested_url_hostname_.c_str(),
            suggested_url_hostname_.c_str(), request_url_hostname_.c_str()));
    GetWebContents().RemoveUserData(UserDataKey());
  }

  void WebContentsDestroyed() override {
    GetWebContents().RemoveUserData(UserDataKey());
  }

  const std::string request_url_hostname_;
  const std::string suggested_url_hostname_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommonNameMismatchRedirectObserver);

void RecordUMA(SSLErrorHandler::UMAEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kHistogram, event,
                            SSLErrorHandler::SSL_ERROR_HANDLER_EVENT_COUNT);
}

bool IsMITMSoftwareInterstitialEnabled() {
  return base::FeatureList::IsEnabled(kMITMSoftwareInterstitial);
}

bool IsSSLCommonNameMismatchHandlingEnabled() {
  return base::FeatureList::IsEnabled(kSSLCommonNameMismatchHandling);
}

// Configuration for SSLErrorHandler.
class ConfigSingleton {
 public:
  ConfigSingleton();

  base::TimeDelta interstitial_delay() const;
  SSLErrorHandler::TimerStartedCallback* timer_started_callback() const;
  base::Clock* clock() const;

  // Returns the name of a known MITM software provider that matches the
  // certificate passed in as the |cert| parameter. Returns empty string if
  // there is no match.
  const std::string MatchKnownMITMSoftware(
      const scoped_refptr<net::X509Certificate> cert);

  // Returns a DynamicInterstitialInfo that matches with |ssl_info|. If is no
  // match, return null.
  std::optional<DynamicInterstitialInfo> MatchDynamicInterstitial(
      const net::SSLInfo& ssl_info,
      bool is_overridable);

  // Testing methods:
  void ResetForTesting();
  void SetInterstitialDelayForTesting(const base::TimeDelta& delay);
  void SetTimerStartedCallbackForTesting(
      SSLErrorHandler::TimerStartedCallback* callback);
  void SetClockForTesting(base::Clock* clock);

  void SetErrorAssistantProto(
      std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
          error_assistant_proto);

  int GetErrorAssistantProtoVersionIdForTesting() const;

 private:
  base::TimeDelta interstitial_delay_;

  // Callback to call when the interstitial timer is started. Used for
  // testing.
  raw_ptr<SSLErrorHandler::TimerStartedCallback, DanglingUntriaged>
      timer_started_callback_ = nullptr;

  // The clock to use when deciding which error type to display. Used for
  // testing.
  raw_ptr<base::Clock, DanglingUntriaged> testing_clock_ = nullptr;

  std::unique_ptr<SSLErrorAssistant> ssl_error_assistant_;
};

ConfigSingleton::ConfigSingleton()
    : interstitial_delay_(base::Milliseconds(kInterstitialDelayInMilliseconds)),
      ssl_error_assistant_(std::make_unique<SSLErrorAssistant>()) {}

base::TimeDelta ConfigSingleton::interstitial_delay() const {
  return interstitial_delay_;
}

SSLErrorHandler::TimerStartedCallback* ConfigSingleton::timer_started_callback()
    const {
  return timer_started_callback_;
}

base::Clock* ConfigSingleton::clock() const {
  return testing_clock_;
}

void ConfigSingleton::ResetForTesting() {
  interstitial_delay_ = base::Milliseconds(kInterstitialDelayInMilliseconds);
  timer_started_callback_ = nullptr;
  testing_clock_ = nullptr;
  ssl_error_assistant_->ResetForTesting();
}

void ConfigSingleton::SetInterstitialDelayForTesting(
    const base::TimeDelta& delay) {
  interstitial_delay_ = delay;
}

void ConfigSingleton::SetTimerStartedCallbackForTesting(
    SSLErrorHandler::TimerStartedCallback* callback) {
  DCHECK(!callback || !callback->is_null());
  timer_started_callback_ = callback;
}

void ConfigSingleton::SetClockForTesting(base::Clock* clock) {
  testing_clock_ = clock;
}

int ConfigSingleton::GetErrorAssistantProtoVersionIdForTesting() const {
  return ssl_error_assistant_->GetErrorAssistantProtoVersionIdForTesting();
}

void ConfigSingleton::SetErrorAssistantProto(
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> proto) {
  ssl_error_assistant_->SetErrorAssistantProto(std::move(proto));
}

const std::string ConfigSingleton::MatchKnownMITMSoftware(
    const scoped_refptr<net::X509Certificate> cert) {
  return ssl_error_assistant_->MatchKnownMITMSoftware(cert);
}

std::optional<DynamicInterstitialInfo>
ConfigSingleton::MatchDynamicInterstitial(const net::SSLInfo& ssl_info,
                                          bool is_overridable) {
  return ssl_error_assistant_->MatchDynamicInterstitial(ssl_info,
                                                        is_overridable);
}

class SSLErrorHandlerDelegateImpl : public SSLErrorHandler::Delegate {
 public:
  SSLErrorHandlerDelegateImpl(
      content::WebContents* web_contents,
      const net::SSLInfo& ssl_info,
      content::BrowserContext* const browser_context,
      net::Error cert_error,
      int options_mask,
      const GURL& request_url,
      std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
      SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback)
      : web_contents_(web_contents),
        ssl_info_(ssl_info),
        browser_context_(browser_context),
        cert_error_(cert_error),
        options_mask_(options_mask),
        request_url_(request_url),
        blocking_page_factory_(std::move(blocking_page_factory)),
        blocking_page_ready_callback_(std::move(blocking_page_ready_callback)) {
    DCHECK(!blocking_page_ready_callback_.is_null());
  }
  ~SSLErrorHandlerDelegateImpl() override;

  // SSLErrorHandler::Delegate methods:
  bool GetSuggestedUrl(const std::vector<std::string>& dns_names,
                       GURL* suggested_url) const override;
  void CheckSuggestedUrl(
      const GURL& suggested_url,
      CommonNameMismatchHandler::CheckUrlCallback callback) override;
  void NavigateToSuggestedURL(const GURL& suggested_url) override;
  bool IsErrorOverridable() const override;
  void ShowMITMSoftwareInterstitial(
      const std::string& mitm_software_name) override;
  void ShowSSLInterstitial(const GURL& support_url) override;
  void ShowBadClockInterstitial(const base::Time& now,
                                ssl_errors::ClockState clock_state) override;
  void ShowBlockedInterceptionInterstitial() override;
  void ShowLocalSelfSignedInterstitial() override;
  bool HasBlockedInterception() const override;

 private:
  // Calls the |blocking_page_ready_callback_| if it's not null, else calls
  // Show() on the given interstitial.
  void OnBlockingPageReady(
      std::unique_ptr<security_interstitials::SecurityInterstitialPage>
          interstitial_page);

  raw_ptr<content::WebContents> web_contents_;
  const net::SSLInfo ssl_info_;
  const raw_ptr<content::BrowserContext> browser_context_;
  const net::Error cert_error_;
  const int options_mask_;
  const GURL request_url_;
  std::unique_ptr<CommonNameMismatchHandler> common_name_mismatch_handler_;
  std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory_;
  SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback_;
};

SSLErrorHandlerDelegateImpl::~SSLErrorHandlerDelegateImpl() {
  if (common_name_mismatch_handler_) {
    common_name_mismatch_handler_->Cancel();
    common_name_mismatch_handler_.reset();
  }
}

bool SSLErrorHandlerDelegateImpl::GetSuggestedUrl(
    const std::vector<std::string>& dns_names,
    GURL* suggested_url) const {
  return CommonNameMismatchHandler::GetSuggestedUrl(request_url_, dns_names,
                                                    suggested_url);
}

void SSLErrorHandlerDelegateImpl::CheckSuggestedUrl(
    const GURL& suggested_url,
    CommonNameMismatchHandler::CheckUrlCallback callback) {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory(
      browser_context_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
  common_name_mismatch_handler_ = std::make_unique<CommonNameMismatchHandler>(
      request_url_, url_loader_factory);

  common_name_mismatch_handler_->CheckSuggestedUrl(suggested_url,
                                                   std::move(callback));
}

void SSLErrorHandlerDelegateImpl::NavigateToSuggestedURL(
    const GURL& suggested_url) {
  content::NavigationController::LoadURLParams load_params(suggested_url);
  load_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  web_contents_->GetController().LoadURLWithParams(load_params);
}

bool SSLErrorHandlerDelegateImpl::IsErrorOverridable() const {
  return SSLBlockingPage::IsOverridable(options_mask_);
}

void SSLErrorHandlerDelegateImpl::ShowMITMSoftwareInterstitial(
    const std::string& mitm_software_name) {
  // Show MITM software blocking page. The interstitial owns the blocking page.
  OnBlockingPageReady(blocking_page_factory_->CreateMITMSoftwareBlockingPage(
      web_contents_, cert_error_, request_url_, ssl_info_, mitm_software_name));
}

void SSLErrorHandlerDelegateImpl::ShowSSLInterstitial(const GURL& support_url) {
  // Show SSL blocking page. The interstitial owns the blocking page.
  OnBlockingPageReady(blocking_page_factory_->CreateSSLPage(
      web_contents_, cert_error_, ssl_info_, request_url_, options_mask_,
      base::Time::NowFromSystemTime(), support_url));
}

void SSLErrorHandlerDelegateImpl::ShowBadClockInterstitial(
    const base::Time& now,
    ssl_errors::ClockState clock_state) {
  // Show bad clock page. The interstitial owns the blocking page.
  OnBlockingPageReady(blocking_page_factory_->CreateBadClockBlockingPage(
      web_contents_, cert_error_, ssl_info_, request_url_, now, clock_state));
}

void SSLErrorHandlerDelegateImpl::ShowBlockedInterceptionInterstitial() {
  // Show interception blocking page. The interstitial owns the blocking page.
  OnBlockingPageReady(
      blocking_page_factory_->CreateBlockedInterceptionBlockingPage(
          web_contents_, cert_error_, request_url_, ssl_info_));
}

void SSLErrorHandlerDelegateImpl::ShowLocalSelfSignedInterstitial() {
  OnBlockingPageReady(blocking_page_factory_->CreateLocalSelfSignedBlockingPage(
      web_contents_, cert_error_, ssl_info_, request_url_, options_mask_,
      base::Time::NowFromSystemTime(), GURL()));
}

bool SSLErrorHandlerDelegateImpl::HasBlockedInterception() const {
  return ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_) ==
         ssl_errors::ErrorInfo::CERT_KNOWN_INTERCEPTION_BLOCKED;
}

void SSLErrorHandlerDelegateImpl::OnBlockingPageReady(
    std::unique_ptr<security_interstitials::SecurityInterstitialPage>
        interstitial_page) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(blocking_page_ready_callback_),
                                std::move(interstitial_page)));
}

ConfigSingleton& GetConfig() {
  static base::NoDestructor<ConfigSingleton> config;
  return *config;
}

}  // namespace

// static
void SSLErrorHandler::HandleSSLError(
    content::WebContents* web_contents,
    net::Error cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    base::OnceCallback<
        void(std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
        blocking_page_ready_callback,
    std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
    bool user_can_proceed_past_interstitial /*=true*/) {
  DCHECK(!FromWebContents(web_contents));

  bool hard_override_disabled = !user_can_proceed_past_interstitial;

  int options_mask = security_interstitials::CalculateSSLErrorOptionsMask(
      cert_error, hard_override_disabled, ssl_info.is_fatal_cert_error);

  auto delegate = std::make_unique<SSLErrorHandlerDelegateImpl>(
      web_contents, ssl_info, web_contents->GetBrowserContext(), cert_error,
      options_mask, request_url, std::move(blocking_page_factory),
      std::move(blocking_page_ready_callback));
  // Protected ctor.
  auto* error_handler = new SSLErrorHandler(std::move(delegate), web_contents,
                                            cert_error, ssl_info, request_url);
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(error_handler));
  error_handler->StartHandlingError();
}

// static
void SSLErrorHandler::ResetConfigForTesting() {
  GetConfig().ResetForTesting();
}

// static
void SSLErrorHandler::SetInterstitialDelayForTesting(
    const base::TimeDelta& delay) {
  GetConfig().SetInterstitialDelayForTesting(delay);
}

// static
void SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(
    TimerStartedCallback* callback) {
  GetConfig().SetTimerStartedCallbackForTesting(callback);
}

// static
void SSLErrorHandler::SetClockForTesting(base::Clock* testing_clock) {
  GetConfig().SetClockForTesting(testing_clock);
}

// static
std::string SSLErrorHandler::GetHistogramNameForTesting() {
  return kHistogram;
}

// static
int SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() {
  return GetConfig().GetErrorAssistantProtoVersionIdForTesting();
}

bool SSLErrorHandler::IsTimerRunningForTesting() const {
  return timer_.IsRunning();
}

// static
void SSLErrorHandler::SetErrorAssistantProto(
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto) {
  GetConfig().SetErrorAssistantProto(std::move(config_proto));
}

SSLErrorHandler::SSLErrorHandler(std::unique_ptr<Delegate> delegate,
                                 content::WebContents* web_contents,
                                 net::Error cert_error,
                                 const net::SSLInfo& ssl_info,
                                 const GURL& request_url)
    : content::WebContentsUserData<SSLErrorHandler>(*web_contents),
      content::WebContentsObserver(web_contents),
      delegate_(std::move(delegate)),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      request_url_(request_url) {}

SSLErrorHandler::~SSLErrorHandler() = default;

void SSLErrorHandler::StartHandlingError() {
  RecordUMA(HANDLE_ALL);

  if (delegate_->HasBlockedInterception()) {
    return ShowBlockedInterceptionInterstitial();
  }

  if (cert_error_ == net::ERR_CERT_SELF_SIGNED_LOCAL_NETWORK) {
    return ShowLocalSelfSignedInterstitial();
  }

  if (ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_) ==
      ssl_errors::ErrorInfo::CERT_DATE_INVALID) {
    HandleCertDateInvalidError();
    return;
  }

  std::optional<DynamicInterstitialInfo> dynamic_interstitial =
      GetConfig().MatchDynamicInterstitial(ssl_info_,
                                           delegate_->IsErrorOverridable());
  if (dynamic_interstitial) {
    ShowDynamicInterstitial(dynamic_interstitial.value());
    return;
  }

  const bool only_error_is_name_mismatch =
      IsOnlyCertError(net::CERT_STATUS_COMMON_NAME_INVALID);

  // The MITM software interstitial is displayed if and only if:
  // - the error thrown is not overridable
  // - the only certificate error is CERT_STATUS_AUTHORITY_INVALID
  // - the certificate contains a string that indicates it was issued by a
  //   MITM software
  if (IsMITMSoftwareInterstitialEnabled() && !delegate_->IsErrorOverridable() &&
      IsOnlyCertError(net::CERT_STATUS_AUTHORITY_INVALID)) {
    const std::string found_mitm_software =
        GetConfig().MatchKnownMITMSoftware(ssl_info_.cert);
    if (!found_mitm_software.empty()) {
      ShowMITMSoftwareInterstitial(found_mitm_software);
      return;
    }
  }

  if (IsSSLCommonNameMismatchHandlingEnabled() &&
      cert_error_ == net::ERR_CERT_COMMON_NAME_INVALID &&
      delegate_->IsErrorOverridable()) {
    std::vector<std::string> dns_names;
    ssl_info_.cert->GetSubjectAltName(&dns_names, nullptr);
    GURL suggested_url;
    if (!dns_names.empty() &&
        delegate_->GetSuggestedUrl(dns_names, &suggested_url)) {
      RecordUMA(WWW_MISMATCH_FOUND_IN_SAN);

      // Show the SSL interstitial if |CERT_STATUS_COMMON_NAME_INVALID| is not
      // the only error.
      if (!only_error_is_name_mismatch) {
        ShowSSLInterstitial();
        return;
      }
      delegate_->CheckSuggestedUrl(
          suggested_url,
          base::BindOnce(&SSLErrorHandler::CommonNameMismatchHandlerCallback,
                         weak_ptr_factory_.GetWeakPtr()));
      timer_.Start(FROM_HERE, GetConfig().interstitial_delay(), this,
                   &SSLErrorHandler::ShowSSLInterstitial);

      if (GetConfig().timer_started_callback()) {
        GetConfig().timer_started_callback()->Run(web_contents());
      }

      return;
    }
  }

  // Display an SSL interstitial.
  ShowSSLInterstitial();
}

void SSLErrorHandler::ShowMITMSoftwareInterstitial(
    const std::string& mitm_software_name) {
  // Show SSL blocking page. The interstitial owns the blocking page.
  RecordUMA(SHOW_MITM_SOFTWARE_INTERSTITIAL);
  delegate_->ShowMITMSoftwareInterstitial(mitm_software_name);
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowSSLInterstitial() {
  // Show SSL blocking page. The interstitial owns the blocking page.
  RecordUMA(delegate_->IsErrorOverridable()
                ? SHOW_SSL_INTERSTITIAL_OVERRIDABLE
                : SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE);
  delegate_->ShowSSLInterstitial(/*support_url=*/GURL());
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowBadClockInterstitial(
    const base::Time& now,
    ssl_errors::ClockState clock_state) {
  RecordUMA(SHOW_BAD_CLOCK);
  delegate_->ShowBadClockInterstitial(now, clock_state);
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowDynamicInterstitial(
    const DynamicInterstitialInfo dynamic_interstitial) {
  switch (dynamic_interstitial.interstitial_type) {
    case chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_NONE:
      NOTREACHED();
    case chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL:
      delegate_->ShowSSLInterstitial(dynamic_interstitial.support_url);
      return;
    case chrome_browser_ssl::DynamicInterstitial::
        INTERSTITIAL_PAGE_MITM_SOFTWARE:
      DCHECK(!dynamic_interstitial.mitm_software_name.empty());
      delegate_->ShowMITMSoftwareInterstitial(
          dynamic_interstitial.mitm_software_name);
      return;
  }
}

void SSLErrorHandler::ShowBlockedInterceptionInterstitial() {
  // Show a blocking page. The interstitial owns the blocking page.
  RecordUMA(SHOW_BLOCKED_INTERCEPTION_INTERSTITIAL);
  delegate_->ShowBlockedInterceptionInterstitial();
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowLocalSelfSignedInterstitial() {
  RecordUMA(SHOW_LOCAL_SELF_SIGNED_INTERSTITIAL);
  delegate_->ShowLocalSelfSignedInterstitial();
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::CommonNameMismatchHandlerCallback(
    CommonNameMismatchHandler::SuggestedUrlCheckResult result,
    const GURL& suggested_url) {
  timer_.Stop();
  if (result == CommonNameMismatchHandler::SuggestedUrlCheckResult::
                    SUGGESTED_URL_AVAILABLE) {
    RecordUMA(WWW_MISMATCH_URL_AVAILABLE);
    CommonNameMismatchRedirectObserver::AddToConsoleAfterNavigation(
        web_contents(), request_url_.GetHost(), suggested_url.GetHost());
    delegate_->NavigateToSuggestedURL(suggested_url);
  } else {
    RecordUMA(WWW_MISMATCH_URL_NOT_AVAILABLE);
    ShowSSLInterstitial();
  }
}

void SSLErrorHandler::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Destroy the error handler on all new navigations. This ensures that the
  // handler is properly recreated when a hanging page is navigated to an SSL
  // error, even when the tab's WebContents doesn't change.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::NavigationStopped() {
  // Destroy the error handler when the page load is stopped.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::DeleteSSLErrorHandler() {
  delegate_.reset();
  // Deletes |this| and also destroys the timer.
  web_contents()->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::HandleCertDateInvalidError() {
  base::Clock* testing_clock = GetConfig().clock();
  const base::Time now =
      testing_clock ? testing_clock->Now() : base::Time::NowFromSystemTime();

  ssl_errors::ClockState clock_state = ssl_errors::GetClockState(now);
  if (clock_state == ssl_errors::CLOCK_STATE_FUTURE ||
      clock_state == ssl_errors::CLOCK_STATE_PAST) {
    ShowBadClockInterstitial(now, clock_state);
    return;  // |this| is deleted after showing the interstitial.
  }
  ShowSSLInterstitial();
}

// Returns true if |only_cert_error_expected| is the only error code present in
// the certificate. The parameter |only_cert_error_expected| is a
// net::CertStatus code representing the most serious error identified on the
// certificate. For example, this could be net::CERT_STATUS_COMMON_NAME_INVALID.
// This function is useful for rendering interstitials that are triggered by one
// specific error code only.
bool SSLErrorHandler::IsOnlyCertError(
    net::CertStatus only_cert_error_expected) const {
  const net::CertStatus other_errors =
      ssl_info_.cert_status ^ only_cert_error_expected;

  return cert_error_ ==
             net::MapCertStatusToNetError(only_cert_error_expected) &&
         !net::IsCertStatusError(other_errors);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SSLErrorHandler);
