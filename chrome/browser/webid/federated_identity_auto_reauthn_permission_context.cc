// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

FederatedIdentityAutoReauthnPermissionContext::
    FederatedIdentityAutoReauthnPermissionContext(
        HostContentSettingsMap* host_content_settings_map,
        permissions::PermissionDecisionAutoBlocker* permission_autoblocker,
        PrefService* pref_service)
    : host_content_settings_map_(host_content_settings_map),
      permission_autoblocker_(permission_autoblocker),
      pref_service_(pref_service) {}

FederatedIdentityAutoReauthnPermissionContext::
    ~FederatedIdentityAutoReauthnPermissionContext() = default;

// static
void FederatedIdentityAutoReauthnPermissionContext::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      password_manager::prefs::kCredentialsEnableAutosignin, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
}

bool FederatedIdentityAutoReauthnPermissionContext::
    IsAutoReauthnSettingEnabled() {
  return pref_service_ &&
         pref_service_->GetBoolean(
             password_manager::prefs::kCredentialsEnableAutosignin) &&
         host_content_settings_map_->GetDefaultContentSetting(
             ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
             /*provider_id=*/nullptr) != ContentSetting::CONTENT_SETTING_BLOCK;
}

bool FederatedIdentityAutoReauthnPermissionContext::IsAutoReauthnEmbargoed(
    const url::Origin& relying_party_embedder) {
  return permission_autoblocker_->IsEmbargoed(
      relying_party_embedder.GetURL(),
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
}

bool FederatedIdentityAutoReauthnPermissionContext::
    IsAutoReauthnDisabledByEmbedder(content::WebContents* web_contents) {
  return false;
}

void FederatedIdentityAutoReauthnPermissionContext::RecordEmbargoForAutoReauthn(
    const url::Origin& relying_party_embedder) {
  const GURL rp_embedder_url = relying_party_embedder.GetURL();
  permission_autoblocker_->RecordDisplayAndEmbargo(
      rp_embedder_url,
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
}

void FederatedIdentityAutoReauthnPermissionContext::RemoveEmbargoForAutoReauthn(
    const url::Origin& relying_party_embedder) {
  const GURL rp_embedder_url = relying_party_embedder.GetURL();
  permission_autoblocker_->RemoveEmbargoAndResetCounts(
      rp_embedder_url,
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
}

void FederatedIdentityAutoReauthnPermissionContext::SetRequiresUserMediation(
    const url::Origin& rp_origin,
    bool requires_user_mediation) {
  const GURL rp_url = rp_origin.GetURL();
  host_content_settings_map_->SetContentSettingDefaultScope(
      rp_url, rp_url,
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
      requires_user_mediation ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW);
}

bool FederatedIdentityAutoReauthnPermissionContext::RequiresUserMediation(
    const url::Origin& rp_origin) {
  const GURL rp_url = rp_origin.GetURL();
  return host_content_settings_map_->GetContentSetting(
             rp_url, rp_url,
             ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION) ==
         ContentSetting::CONTENT_SETTING_BLOCK;
}
