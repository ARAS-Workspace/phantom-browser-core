// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Threat types, threat sources and threat metadata used by the security
// interstitial code.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_THREAT_ENUMS_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_THREAT_ENUMS_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/trace_event/traced_value.h"

namespace safe_browsing {

// Different types of threats that SafeBrowsing protects against. This is the
// type that's returned to the clients of SafeBrowsing in Chromium.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SBThreatType {
  // This type can be used for lists that can be checked synchronously so a
  // client callback isn't required, or for allowlists.
  SB_THREAT_TYPE_UNUSED = 0,

  // No threat at all.
  SB_THREAT_TYPE_SAFE = 1,

  // The URL is being used for phishing.
  SB_THREAT_TYPE_URL_PHISHING = 2,

  // The URL hosts malware.
  SB_THREAT_TYPE_URL_MALWARE = 3,

  // The URL hosts unwanted programs.
  SB_THREAT_TYPE_URL_UNWANTED = 4,

  // The download URL is malware.
  SB_THREAT_TYPE_URL_BINARY_MALWARE = 5,

  // Url detected by the client-side phishing model or the on-device model. Note
  // that unlike the above values, this does not correspond to a downloaded
  // list.
  SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING = 6,

  // The Chrome extension or app (given by its ID) is malware.
  SB_THREAT_TYPE_EXTENSION = 7,

  // DEPRECATED. Url detected by the client-side malware IP list. This IP list
  // is part of the client side detection model.
  DEPRECATED_SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE = 8,

  // Url leads to a blocklisted resource script. Note that no warnings should be
  // shown on this threat type, but an incident report might be sent.
  // DEPRECATED: SB_THREAT_TYPE_BLOCKLISTED_RESOURCE = 9,

  // Url abuses a permission API.
  SB_THREAT_TYPE_API_ABUSE = 10,

  // Activation patterns for the Subresource Filter.
  SB_THREAT_TYPE_SUBRESOURCE_FILTER = 11,

  // CSD Phishing allowlist.  This "threat" means a URL matched the allowlist.
  SB_THREAT_TYPE_CSD_ALLOWLIST = 12,

  // DEPRECATED. Url detected by password protection service.
  DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING = 13,

  // Saved password reuse detected on low reputation page,
  SB_THREAT_TYPE_SAVED_PASSWORD_REUSE = 14,

  // Chrome signed in and syncing gaia password reuse detected on low reputation
  // page,
  SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE = 15,

  // Chrome signed in non syncing gaia password reuse detected on low reputation
  // page,
  SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE = 16,

  // A Google ad that caused a blocked autoredirect was collected
  SB_THREAT_TYPE_BLOCKED_AD_REDIRECT = 17,

  // A sample of an ad was collected
  SB_THREAT_TYPE_AD_SAMPLE = 18,

  // A report of Google ad that caused a blocked popup was collected.
  SB_THREAT_TYPE_BLOCKED_AD_POPUP = 19,

  // This site is considered suspicious enough to trigger logging a report to
  // Safe Browsing with more details.
  // TODO(awado): Rename this to SB_THREAT_TYPE_LOGGABLE_SUSPICIOUS_SITE.
  SB_THREAT_TYPE_SUSPICIOUS_SITE = 20,

  // Enterprise password reuse detected on low reputation page.
  SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE = 21,

  // Potential billing detected.
  SB_THREAT_TYPE_BILLING = 22,

  // Off-market APK file downloaded, which could be potentially dangerous.
  SB_THREAT_TYPE_APK_DOWNLOAD = 23,

  // Match found in the local high-confidence allowlist.
  SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST = 24,

  // List of URLs that should shown an accuracy tip.
  // DEPRECATED: SB_THREAT_TYPE_ACCURACY_TIPS = 25,

  // Managed policy indicated to warn a navigation.
  SB_THREAT_TYPE_MANAGED_POLICY_WARN = 26,

  // Managed policy indicated to block a navigation.
  SB_THREAT_TYPE_MANAGED_POLICY_BLOCK = 27,

  // CSD Download allowlist.
  SB_THREAT_TYPE_CSD_DOWNLOAD_ALLOWLIST = 28,

  // Sites that are classified as suspicious by SafeBrowsing and should display
  // a nonblocking warning.
  SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE = 29,

  kMaxValue = SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE,

};

using SBThreatTypeSet = base::flat_set<SBThreatType>;

// What service classified this threat as unsafe.
enum class ThreatSource {
  UNKNOWN,
  // From SBLocalDatabaseManager, protocol v4. Desktop only.
  // TODO(crbug.com/372395685): delete upon v4 deprecation.
  LOCAL_PVER4,
  // From ClientSideDetectionHost.
  CLIENT_SIDE_DETECTION,
  // From RealTimeUrlLookupService. Not including fallback to protocol v4.
  URL_REAL_TIME_CHECK,
  // From HashRealTimeService. Desktop only. Not including fallback to
  // protocol v4.
  NATIVE_PVER5_REAL_TIME,
  // From GmsCore SafeBrowsing API. Android only. Including fallback to protocol
  // v4 (through either SafeBrowsing API or SafetyNet API).
  ANDROID_SAFEBROWSING_REAL_TIME,
  // From GmsCore SafeBrowsing API. Android only. Protocol v4 only.
  ANDROID_SAFEBROWSING,
  // Triggered by Glic web client when server reports a dangerous Counter
  // Abuse verdict.
  GLIC_COUNTER_ABUSE,
  // From SBLocalDatabaseManager, protocol v5. Desktop only.
  LOCAL_PVER5_LOCAL_BLOCKLIST,
};

// What subtype that expands more into details on what threat category
// SBThreatType is targeting.
enum class ThreatSubtype {
  UNKNOWN,
  // Scam experiment verdict 1
  SCAM_EXPERIMENT_VERDICT_1,
  // Scam experiment verdict 2
  SCAM_EXPERIMENT_VERDICT_2,
  // Scam experiment verdict 3
  SCAM_EXPERIMENT_VERDICT_3,
  // Scam experiment verdict 4
  SCAM_EXPERIMENT_VERDICT_4,
  // Scam experiment catch all enforcement
  SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT,
};

enum class SubresourceFilterType : int { ABUSIVE = 0, BETTER_ADS = 1 };

// Levels of enforcement for subresource filtering. These values must remain
// ordered by increasing severity (e.g. ENFORCE is more severe than WARN)
// because comparisons rely on this ordering.
enum class SubresourceFilterLevel : int { WARN = 0, ENFORCE = 1 };

using SubresourceFilterMatch =
    base::flat_map<SubresourceFilterType, SubresourceFilterLevel>;

// Metadata that was returned by a GetFullHash call. This is the parsed version
// of the PB (from Pver3, or Pver4 local) or JSON (from Pver4 via GMSCore).
// Some fields are only applicable to certain lists.
// When adding elements to this struct, make sure you update ToTracedValue.
struct ThreatMetadata {
  ThreatMetadata();
  ThreatMetadata(const ThreatMetadata& other);
  ThreatMetadata(ThreatMetadata&& other);
  ThreatMetadata& operator=(const ThreatMetadata& other);
  ThreatMetadata& operator=(ThreatMetadata&& other);
  ~ThreatMetadata();

  friend bool operator==(const ThreatMetadata&,
                         const ThreatMetadata&) = default;

  // Returns the metadata in a format tracing can support.
  std::unique_ptr<base::trace_event::TracedValue> ToTracedValue() const;

  // Map of list sub-types related to the SUBRESOURCE_FILTER threat type.
  SubresourceFilterMatch subresource_filter_match;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_THREAT_ENUMS_H_
