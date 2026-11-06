// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_histograms.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"

namespace trusted_vault {

void RecordTrustedVaultHintDegradedRecoverabilityChangedReason(
    TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA
        hint_degraded_recoverability_changed_reason) {
  base::UmaHistogramEnumeration(
      "TrustedVault.TrustedVaultHintDegradedRecoverabilityChangedReason",
      hint_degraded_recoverability_changed_reason);
}

void RecordTrustedVaultRecoveryFactorRegistrationState(
    LocalRecoveryFactorType local_recovery_factor_type,
    SecurityDomainId security_domain_id,
    TrustedVaultRecoveryFactorRegistrationStateForUMA registration_state) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"TrustedVault.RecoveryFactorRegistrationState.",
           GetLocalRecoveryFactorNameForUma(local_recovery_factor_type), ".",
           GetSecurityDomainNameForUma(security_domain_id)}),
      registration_state);
}

void RecordTrustedVaultRecoveryFactorRegistrationOutcome(
    LocalRecoveryFactorType local_recovery_factor_type,
    SecurityDomainId security_domain_id,
    TrustedVaultRecoveryFactorRegistrationOutcomeForUMA registration_outcome) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"TrustedVault.RecoveryFactorRegistrationOutcome.",
           GetLocalRecoveryFactorNameForUma(local_recovery_factor_type), ".",
           GetSecurityDomainNameForUma(security_domain_id)}),
      registration_outcome);
}

void RecordTrustedVaultDownloadKeysStatus(
    LocalRecoveryFactorType local_recovery_factor_type,
    SecurityDomainId security_domain_id,
    TrustedVaultDownloadKeysStatusForUMA status) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"TrustedVault.DownloadKeysStatus.",
           GetLocalRecoveryFactorNameForUma(local_recovery_factor_type), ".",
           GetSecurityDomainNameForUma(security_domain_id)}),
      status);
}

void RecordTrustedVaultDownloadKeysStatus(
    TrustedVaultDownloadKeysStatusForUMA status) {
  RecordTrustedVaultDownloadKeysStatus(LocalRecoveryFactorType::kPhysicalDevice,
                                       SecurityDomainId::kChromeSync, status);
}

void RecordTrustedVaultRecoverKeysOutcome(
    SecurityDomainId security_domain_id,
    TrustedVaultRecoverKeysOutcomeForUMA status) {
  base::UmaHistogramEnumeration(
      base::StrCat({"TrustedVault.RecoverKeysOutcome.",
                    GetSecurityDomainNameForUma(security_domain_id)}),
      status);
}

void RecordTrustedVaultFileReadStatus(SecurityDomainId security_domain_id,
                                      TrustedVaultFileReadStatusForUMA status) {
  base::UmaHistogramEnumeration(
      "TrustedVault.FileReadStatus." +
          GetSecurityDomainNameForUma(security_domain_id),
      status);
}

std::string GetLocalRecoveryFactorNameForUma(
    LocalRecoveryFactorType local_recovery_factor_type) {
  // These strings get embedded in histogram names and so should not be
  // changed.
  switch (local_recovery_factor_type) {
    case LocalRecoveryFactorType::kPhysicalDevice:
      return "PhysicalDevice";
#if BUILDFLAG(IS_MAC)
    case LocalRecoveryFactorType::kICloudKeychain:
      return "ICloudKeychain";
#endif
      // If adding a new value, also update the variants for
      // LocalRecoveryFactorType in
      // tools/metrics/histograms/metadata/trusted_vault/histograms.xml.
  }
}

std::string GetSecurityDomainNameForUma(SecurityDomainId domain) {
  switch (domain) {
    // These strings get embedded in histogram names and so should not be
    // changed.
    case SecurityDomainId::kChromeSync:
      return "ChromeSync";
    case SecurityDomainId::kPasskeys:
      return "HwProtected";
      // If adding a new value, also update the variants for SecurityDomainId
      // in tools/metrics/histograms/metadata/trusted_vault/histograms.xml.
  }
}

void RecordTrustedVaultRecoveryFlowTriggeredEndpoint(
    TrustedVaultRecoveryFlowEndpoint endpoint) {
  base::UmaHistogramEnumeration("TrustedVault.RecoveryFlowTriggeredEndpoint",
                                endpoint);
}

}  // namespace trusted_vault
