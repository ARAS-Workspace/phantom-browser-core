// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"

using one_time_tokens::ExpiringSubscriptionHandle;
using one_time_tokens::OneTimeToken;
using one_time_tokens::OneTimeTokenRetrievalError;
using one_time_tokens::OneTimeTokenService;
using one_time_tokens::OneTimeTokenSource;
using one_time_tokens::OneTimeTokenType;

namespace autofill {

namespace {
constexpr base::TimeDelta kSubscriptionDuration = base::Minutes(1);
}  // namespace

OtpManagerImpl::OtpManagerImpl(BrowserAutofillManager& owner,
                               OneTimeTokenService* one_time_token_service)
    : owner_(owner), one_time_token_services_(one_time_token_service) {
  autofill_manager_observation_.Observe(&owner);
  if (one_time_token_services_ && one_time_token_services_->log_sink()) {
    log_subscription_ =
        one_time_token_services_->log_sink()->AddLogHandler(base::BindRepeating(
            &OtpManagerImpl::OnLogMessage, weak_ptr_factory_.GetWeakPtr()));
  }
}

OtpManagerImpl::~OtpManagerImpl() = default;

void OtpManagerImpl::GetOtpSuggestions(
    const FormStructure& form,
    const url::Origin& origin,
    OtpManagerImpl::GetOtpSuggestionsCallback callback) {
  if (!OtpFieldDetector::IsOtpForm(form)) {
    std::move(callback).Run({});
    return;
  }

  // TODO(crbug.com/415273270): Do not fill OTP suggestions into opaque origin
  // iframes.
  last_pending_get_suggestions_callback_ = std::move(callback);

  // This queries OTPs from the backend and calls `OnOneTimeTokenReceived` to
  // deliver the OTP to `last_pending_get_suggestions_callback_`.
  GetRecentOtpsAndRenewSubscription();
}

void OtpManagerImpl::GetRecentOtpsAndRenewSubscription() {
  if (!one_time_token_services_) {
    return;
  }

  one_time_token_services_->GetRecentOneTimeTokens(base::BindRepeating(
      &OtpManagerImpl::OnOneTimeTokenReceived, weak_ptr_factory_.GetWeakPtr()));

  if (subscription_.IsAlive()) {
    subscription_.SetExpirationTime(base::Time::Now() + kSubscriptionDuration);
    return;
  }

  subscription_ = one_time_token_services_->Subscribe(
      OneTimeTokenSource::kOnDeviceSms,
      base::Time::Now() + kSubscriptionDuration,
      base::BindRepeating(&OtpManagerImpl::OnOneTimeTokenReceived,
                          weak_ptr_factory_.GetWeakPtr()),
      /*expiration_callback=*/base::DoNothing());
}

void OtpManagerImpl::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form_id,
    AutofillManager::Observer::FieldTypeSource source,
    bool small_forms_were_parsed) {
  // On non-android platforms and in tests the backend may be not initialized.
  if (!one_time_token_services_) {
    return;
  }

  const FormStructure* form = manager.FindCachedFormById(form_id);
  if (!form) {
    return;
  }

  if (!OtpFieldDetector::IsOtpForm(*form)) {
    return;
  }

  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens << "OTP field detected in web form."
      << Br{} << "Form ID: " << form_id;

  GetRecentOtpsAndRenewSubscription();
}

// This is a workaround to prevent the Keyboard Accessory from popping up when
// an OTP arrives and the keyboard is hidden.
// TODO(crbug.com/451991285): Remove this method once we switch to using
// observers instead of delaying the callback.
void OtpManagerImpl::OnBeforeFocusOnFormField(AutofillManager& manager,
                                              FormGlobalId form,
                                              FieldGlobalId field) {
  if (last_pending_get_suggestions_callback_) {
    // Post the callback asynchronously to prevent re-entrancy when notifying
    // `Observer::OnAfterAskForValuesToFill` from inside this
    // `Observer::OnBeforeFocusOnFormField` notification loop.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(last_pending_get_suggestions_callback_),
                       std::vector<std::string>{}));
  }
}

// This is a workaround to prevent the Keyboard Accessory from popping up when
// an OTP arrives and the keyboard is hidden.
// TODO(crbug.com/451991285): Remove this method once we switch to using
// observers instead of delaying the callback.
void OtpManagerImpl::OnBeforeFocusOnNonFormField(AutofillManager& manager) {
  if (last_pending_get_suggestions_callback_) {
    // Post the callback asynchronously to prevent re-entrancy when notifying
    // `Observer::OnAfterAskForValuesToFill` from inside this
    // `Observer::OnBeforeFocusOnNonFormField` notification loop.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(last_pending_get_suggestions_callback_),
                       std::vector<std::string>{}));
  }
}

void OtpManagerImpl::OnOneTimeTokenReceived(
    OneTimeTokenSource backend_type,
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> token_or_error) {
  // If token_or_error holds an error, run the callback with empty otp value.
  if (!token_or_error.has_value()) {
    if (!last_pending_get_suggestions_callback_.is_null()) {
      std::move(last_pending_get_suggestions_callback_).Run({});
    }
    return;
  }

  // If we are here, token_or_error holds a OneTimeToken, we check if the
  // callback is valid.
  if (!last_pending_get_suggestions_callback_) {
    return;
  }

  OneTimeToken& token = token_or_error.value();
  if (!token.value().empty()) {
    owner_->GetOtpFormEventLogger().OnOtpAvailable();
  }

  base::UmaHistogramBoolean("Autofill.OneTimeTokens.PhishGuard.CheckPerformed",
                            false);
  MaybeShowOtpSuggestions(std::move(token),
                          OneTimeTokensPhishGuardVerdict::kUnknown);
}

void OtpManagerImpl::MaybeShowOtpSuggestions(
    OneTimeToken token,
    OneTimeTokensPhishGuardVerdict verdict) {
  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens
      << "PhishGuard check completed with verdict: " << verdict;

  base::UmaHistogramEnumeration("Autofill.OneTimeTokens.PhishGuard.Verdict",
                                verdict);

  if (!last_pending_get_suggestions_callback_) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "No pending callback, skipping further processing.";
    return;
  }

  std::vector<std::string> suggestions;
  if (!token.value().empty()) {
    suggestions.emplace_back(std::move(token).value());
  }

  if (IsOtpDeliveryBlocked()) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens << LogMessage::kSuggestionSuppressed
        << "Reason: OTP delivery is blocked due to the WebOTP API.";
    suggestions.clear();
  } else if (verdict == OneTimeTokensPhishGuardVerdict::kPhishing) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens << LogMessage::kSuggestionSuppressed
        << "Reason: PhishGuard verdict is phishing.";
    suggestions.clear();
  } else if (!suggestions.empty()) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "Delivering OTP suggestion to UI. Token length: "
        << suggestions[0].size() << " (value omitted for privacy).";
  }

  std::move(last_pending_get_suggestions_callback_).Run(std::move(suggestions));
}

bool OtpManagerImpl::IsOtpDeliveryBlocked() {
  return owner_->client().DocumentUsedWebOTP();
}

std::optional<one_time_tokens::OneTimeToken>
OtpManagerImpl::SelectMostRecentToken() const {
  if (received_otps_.empty()) {
    return std::nullopt;
  }
  return *std::ranges::max_element(
      received_otps_, {},
      &one_time_tokens::OneTimeToken::on_device_arrival_time);
}

void OtpManagerImpl::OnLogMessage(std::string_view message) {
  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens << message;
}

LogBuffer& operator<<(LogBuffer& buffer,
                      OneTimeTokensPhishGuardVerdict verdict) {
  switch (verdict) {
    case OneTimeTokensPhishGuardVerdict::kUnknown:
      return buffer << "kUnknown";
    case OneTimeTokensPhishGuardVerdict::kPhishing:
      return buffer << "kPhishing";
    case OneTimeTokensPhishGuardVerdict::kNotPhishing:
      return buffer << "kNotPhishing";
  }
  NOTREACHED();
}

}  // namespace autofill
