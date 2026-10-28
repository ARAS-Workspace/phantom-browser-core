// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/webid/federated_identity_auto_reauthn_permission_context_delegate.h"

namespace permissions {
class PermissionDecisionAutoBlocker;
}

namespace url {
class Origin;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class HostContentSettingsMap;
class PrefService;

// Context for storing user permission to use the browser FedCM API's auto
// sign-in feature.
class FederatedIdentityAutoReauthnPermissionContext
    : public content::FederatedIdentityAutoReauthnPermissionContextDelegate,
      public KeyedService {
 public:
  // DO NOT pass a Profile here. Inject other keyed service dependencies
  // explicitly and add a corresponding DependsOn() in the factory. See
  // crbug.com/368297674.
  // `host_content_settings_map` and `permission_autoblocker` must be non-null
  // and outlive this service. `pref_service` is null for non-regular profiles,
  // which report the auto re-authn setting as disabled; otherwise it must
  // outlive this service.
  FederatedIdentityAutoReauthnPermissionContext(
      HostContentSettingsMap* host_content_settings_map,
      permissions::PermissionDecisionAutoBlocker* permission_autoblocker,
      PrefService* pref_service);

  FederatedIdentityAutoReauthnPermissionContext(
      const FederatedIdentityAutoReauthnPermissionContext&) = delete;
  FederatedIdentityAutoReauthnPermissionContext& operator=(
      const FederatedIdentityAutoReauthnPermissionContext&) = delete;

  ~FederatedIdentityAutoReauthnPermissionContext() override;

  // Registers the auto sign-in pref read by IsAutoReauthnSettingEnabled().
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // content::FederatedIdentityAutoReauthnPermissionContextDelegate:
  bool IsAutoReauthnSettingEnabled() override;
  bool IsAutoReauthnEmbargoed(
      const url::Origin& relying_party_embedder) override;
  bool IsAutoReauthnDisabledByEmbedder(
      content::WebContents* web_contents) override;
  void RecordEmbargoForAutoReauthn(
      const url::Origin& relying_party_embedder) override;
  void RemoveEmbargoForAutoReauthn(
      const url::Origin& relying_party_embedder) override;
  void SetRequiresUserMediation(const url::Origin& rp_origin,
                                bool requires_user_mediation) override;
  bool RequiresUserMediation(const url::Origin& rp_origin) override;

 private:
  const raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  const raw_ptr<permissions::PermissionDecisionAutoBlocker>
      permission_autoblocker_;
  const raw_ptr<PrefService> pref_service_;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_
