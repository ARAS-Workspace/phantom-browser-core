// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo_util.h"

#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/features.h"
#include "components/sync_bookmarks/switches.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/base/network_change_notifier.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "components/sync/service/sync_prefs.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/sync/extension_sync_util.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_quality/addresses/address_import_requirement_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace signin {

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

using signin::SignInPromoType;
using signin_util::SignedInState;

constexpr int kSigninPromoShownThreshold = 5;
constexpr int kSigninPromoDismissedThreshold = 2;

syncer::DataType GetDataTypeFromSignInPromoType(SignInPromoType type) {
  switch (type) {
    case SignInPromoType::kPassword:
      return syncer::PASSWORDS;
    case SignInPromoType::kAddress:
      return syncer::CONTACT_INFO;
    case SignInPromoType::kBookmark:
      return syncer::BOOKMARKS;
    case SignInPromoType::kExtension:
      return syncer::EXTENSIONS;
    case SignInPromoType::kSendTabToSelf:
      return syncer::SEND_TAB_TO_SELF;
  }
}

bool PromoTypeHasSyncableData(SignInPromoType type) {
  switch (type) {
    case SignInPromoType::kPassword:
    case SignInPromoType::kAddress:
    case SignInPromoType::kBookmark:
    case SignInPromoType::kExtension:
    case SignInPromoType::kSendTabToSelf:
      return true;
  }
  NOTREACHED();
}

int GetAddressPromoShownCount(Profile& profile, const GaiaId& gaia_id) {
  if (!gaia_id.empty()) {
    return SigninPrefs(*profile.GetPrefs())
        .GetAddressSigninPromoImpressionCount(gaia_id);
  }

  return profile.GetPrefs()->GetInteger(
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? prefs::kAddressSignInPromoShownCountPerProfileForLimitsExperiment
          : prefs::kAddressSignInPromoShownCountPerProfile);
}

int GetPasswordPromoShownCount(Profile& profile, const GaiaId& gaia_id) {
  if (!gaia_id.empty()) {
    return SigninPrefs(*profile.GetPrefs())
        .GetPasswordSigninPromoImpressionCount(gaia_id);
  }

  return profile.GetPrefs()->GetInteger(
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? prefs::kPasswordSignInPromoShownCountPerProfileForLimitsExperiment
          : prefs::kPasswordSignInPromoShownCountPerProfile);
}

int GetBookmarkPromoShownCount(Profile& profile, const GaiaId& gaia_id) {
  if (!gaia_id.empty()) {
    return SigninPrefs(*profile.GetPrefs())
        .GetBookmarkSigninPromoImpressionCount(gaia_id);
  }

  return profile.GetPrefs()->GetInteger(
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? prefs::kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment
          : prefs::kBookmarkSignInPromoShownCountPerProfile);
}

int GetContextualPromoDismissCountPerSignedOutProfile(Profile& profile,
                                                      SignInPromoType type) {
  if (ShouldUseAutofillSignInPromoLimits(type)) {
    return profile.GetPrefs()->GetInteger(
        prefs::kAutofillSignInPromoDismissCountPerProfile);
  }

  switch (type) {
    case SignInPromoType::kAddress:
      return profile.GetPrefs()->GetInteger(
          prefs::kAddressSignInPromoDismissCountPerProfileForLimitsExperiment);
    case SignInPromoType::kPassword:
      return profile.GetPrefs()->GetInteger(
          prefs::kPasswordSignInPromoDismissCountPerProfileForLimitsExperiment);
    case SignInPromoType::kBookmark:
      return profile.GetPrefs()->GetInteger(
          prefs::kBookmarkSignInPromoDismissCountPerProfileForLimitsExperiment);
    case SignInPromoType::kExtension:
    case SignInPromoType::kSendTabToSelf:
      NOTREACHED();
  }
}

int GetContextualPromoDismissCountPerAccount(Profile& profile,
                                             SignInPromoType type,
                                             const GaiaId& gaia_id) {
  if (ShouldUseAutofillSignInPromoLimits(type)) {
    return SigninPrefs(*profile.GetPrefs())
        .GetAutofillSigninPromoDismissCount(gaia_id);
  }

  switch (type) {
    case SignInPromoType::kAddress:
      return SigninPrefs(*profile.GetPrefs())
          .GetAddressSigninPromoDismissCount(gaia_id);
    case SignInPromoType::kPassword:
      return SigninPrefs(*profile.GetPrefs())
          .GetPasswordSigninPromoDismissCount(gaia_id);
    case SignInPromoType::kBookmark:
      return SigninPrefs(*profile.GetPrefs())
          .GetBookmarkSigninPromoDismissCount(gaia_id);
    case SignInPromoType::kExtension:
    case SignInPromoType::kSendTabToSelf:
      NOTREACHED();
  }
}

bool ShouldShowPromoBasedOnImpressionOrDismissalCount(Profile& profile,
                                                      SignInPromoType type) {
  // Footer sign in promos are always shown.
  if (type == signin::SignInPromoType::kExtension ||
      type == signin::SignInPromoType::kSendTabToSelf ||
      (type == signin::SignInPromoType::kBookmark &&
       !base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp))) {
    return true;
  }

  AccountInfo account = signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(&profile),
      AccountPreviewDataServiceFactory::GetForProfile(&profile));

  int show_count = 0;
  switch (type) {
    case SignInPromoType::kAddress:
      show_count = GetAddressPromoShownCount(profile, account.gaia);
      break;
    case SignInPromoType::kPassword:
      show_count = GetPasswordPromoShownCount(profile, account.gaia);
      break;
    case SignInPromoType::kBookmark:
      if (!base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
        NOTREACHED();
      }
      show_count = GetBookmarkPromoShownCount(profile, account.gaia);
      break;
    case SignInPromoType::kExtension:
    case SignInPromoType::kSendTabToSelf:
      NOTREACHED();
  }

  int dismiss_count =
      account.gaia.empty()
          ? GetContextualPromoDismissCountPerSignedOutProfile(profile, type)
          : GetContextualPromoDismissCountPerAccount(profile, type,
                                                     account.gaia);

  if (base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)) {
    return show_count < switches::kContextualSigninPromoShownThreshold.Get() &&
           dismiss_count <
               switches::kContextualSigninPromoDismissedThreshold.Get();
  }

  // Don't show the promo again if:
  // - it has already been shown `kSigninPromoShownThreshold` times for its
  // autofill bubble promo type.
  // - it has already been dismissed `kSigninPromoDismissedThreshold` times,
  // regardless of autofill bubble promo type.
  return show_count < kSigninPromoShownThreshold &&
         dismiss_count < kSigninPromoDismissedThreshold;
}

bool IsDataTypeManagedByPolicy(const syncer::SyncService* sync_service,
                               syncer::DataType data_type) {
  if (!sync_service) {
    return false;
  }
  std::optional<syncer::UserSelectableType> selectable_type =
      syncer::GetUserSelectableTypeFromDataType(data_type);
  return selectable_type.has_value() &&
         sync_service->GetUserSettings()->IsTypeManagedByPolicy(
             *selectable_type);
}

// Common eligibility checks for signin promos relating to the syncing of an
// underlying syncable data type.
bool CanShowPromoForSyncableDataType(SignInPromoType type, Profile& profile) {
  syncer::SyncPrefs prefs(profile.GetPrefs());
  // Don't show if sync is not allowed to start or is running in local mode.
  if (!SyncServiceFactory::IsSyncAllowed(&profile) ||
      prefs.IsLocalSyncEnabled()) {
    return false;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(&profile);

  // Don't show the promo if the sync service is not available, e.g. if the
  // profile is off-the-record.
  if (!sync_service) {
    return false;
  }

  syncer::DataType data_type = GetDataTypeFromSignInPromoType(type);

  // Don't show the promo if policies disallow account storage.
  if (IsDataTypeManagedByPolicy(sync_service, data_type) ||
      !sync_service->GetDataTypesForTransportOnlyMode().Has(data_type)) {
    return false;
  }
  return true;
}

// Performs base checks for whether the sign in promos should be shown.
// Needs additional checks depending on the type of the promo.
// `profile` is the profile of the tab the promo would be shown on.
bool ShouldShowSignInPromoCommon(Profile& profile, SignInPromoType type) {
  if (profile.IsOffTheRecord()) {
    return false;
  }

  // Don't bother if we don't have any kind of network connection.
  if (net::NetworkChangeNotifier::IsOffline()) {
    return false;
  }

  // Consider original profile even if an off-the-record profile was
  // passed to this method as sign-in state is only defined for the
  // primary profile.
  Profile* original_profile = profile.GetOriginalProfile();

  // Don't show for supervised child profiles.
  if (original_profile->IsChild()) {
    return false;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(original_profile);
  AccountInfo promo_account = signin_ui_util::GetSingleAccountForPromos(
      identity_manager,
      AccountPreviewDataServiceFactory::GetForProfile(original_profile));

  // Don't show if sign in can't be offered (ex: signin disallowed).
  if (!CanOfferSignin(original_profile, promo_account.gaia, promo_account.email,
                      /*allow_account_from_other_profile=*/true)
           .IsOk()) {
    return false;
  }

  if (PromoTypeHasSyncableData(type) &&
      !CanShowPromoForSyncableDataType(type, profile)) {
    return false;
  }

  SignedInState signed_in_state = signin_util::GetSignedInState(
      IdentityManagerFactory::GetForProfile(&profile));

  switch (signed_in_state) {
    case signin_util::SignedInState::kSignedIn:
    case signin_util::SignedInState::kSyncing:
    case signin_util::SignedInState::kSyncPaused:
      // Don't show the promo if the user is already signed in or syncing.
      return false;
    case signin_util::SignedInState::kSignInPending:
      // Always show the promo in sign in pending state.
      return true;
    case signin_util::SignedInState::kSignedOut:
    case signin_util::SignedInState::kWebOnlySignedIn:
      break;
  }

  return ShouldShowPromoBasedOnImpressionOrDismissalCount(profile, type);
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

#if BUILDFLAG(ENABLE_EXTENSIONS)
bool ShouldShowExtensionSignInPromo(Profile& profile,
                                    const extensions::Extension& extension) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!extensions::sync_util::ShouldSync(&profile, &extension)) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
    // `ShouldShowSignInPromoCommon()` does not check if extensions are syncing
    // in transport mode. That's why `IsSyncingExtensionsEnabled()` is added so
    // the sign in promo is not shown in that case.
    if (extensions::sync_util::IsSyncingExtensionsEnabled(&profile)) {
      return false;
    }

    if (const signin::IdentityManager* identity_manager =
            IdentityManagerFactory::GetForProfile(&profile);
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      // The promo is not shown to users that have explicitly signed in through
      // the browser (even if extensions are not syncing).
      return false;
    }
  }

  return ShouldShowSignInPromoCommon(profile, SignInPromoType::kExtension);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

bool ShouldShowPasswordSignInPromo(Profile& profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  return ShouldShowSignInPromoCommon(profile, SignInPromoType::kPassword);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool ShouldShowAddressSignInPromo(Profile& profile,
                                  const autofill::AutofillProfile& address) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Don't show the promo if the new address is not eligible for account
  // storage.
  if (!autofill::IsProfileEligibleForMigrationToAccount(
          autofill::PersonalDataManagerFactory::GetForBrowserContext(&profile)
              ->address_data_manager(),
          address)) {
    return false;
  }

  return ShouldShowSignInPromoCommon(profile, SignInPromoType::kAddress);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool ShouldShowBookmarkSignInPromo(Profile& profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!ShouldShowSignInPromoCommon(profile, SignInPromoType::kBookmark)) {
    return false;
  }

  // At this point, both the identity manager and sync service should not be
  // null.
  IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(&profile);
  CHECK(identity_manager);
  CHECK(sync_service);

  // If the user is in sign in pending state, the promo should only be shown if
  // they already have account storage for bookmarks enabled.
  // Uno Phase 2 Follow up: Always display the promotion.
  return base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp) ||
         !signin_util::IsSigninPending(identity_manager) ||
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kBookmarks);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool IsBubbleSigninPromo(signin_metrics::AccessPoint access_point) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  return access_point == signin_metrics::AccessPoint::kPasswordBubble ||
         access_point == signin_metrics::AccessPoint::kAddressBubble ||
         (base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp) &&
          access_point == signin_metrics::AccessPoint::kBookmarkBubble);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool IsSignInPromo(signin_metrics::AccessPoint access_point) {
  if (
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      // Remove this condition when `syncer::kUnoPhase2FollowUp` is launched as
      // it is already checked in `IsBubbleSigninPromo()`.
      access_point == signin_metrics::AccessPoint::kBookmarkBubble ||
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
      IsBubbleSigninPromo(access_point)) {
    return true;
  }

  if (access_point == signin_metrics::AccessPoint::kExtensionInstallBubble) {
#if BUILDFLAG(IS_CHROMEOS)
    return base::FeatureList::IsEnabled(
        syncer::kReplaceSyncPromosWithSignInPromos);
#else
    return true;
#endif
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (access_point == signin_metrics::AccessPoint::kSendTabToSelfPromo) {
    return true;
  }
#endif

  return false;
}

SignInPromoType GetSignInPromoTypeFromAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kPasswordBubble:
      return SignInPromoType::kPassword;
    case signin_metrics::AccessPoint::kAddressBubble:
      return SignInPromoType::kAddress;
    case signin_metrics::AccessPoint::kBookmarkBubble:
      return SignInPromoType::kBookmark;
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
      return SignInPromoType::kExtension;
    case signin_metrics::AccessPoint::kSendTabToSelfPromo:
      return SignInPromoType::kSendTabToSelf;
    default:
      NOTREACHED();
  }
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void RecordSignInPromoShown(signin_metrics::AccessPoint access_point,
                            Profile* profile) {
  CHECK(profile);
  CHECK(!profile->IsOffTheRecord());

  AccountInfo account = signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(profile),
      AccountPreviewDataServiceFactory::GetForProfile(profile));
  SignInPromoType promo_type = GetSignInPromoTypeFromAccessPoint(access_point);

  // Record the pref per profile if there is no account present.
  if (account.gaia.empty()) {
    const char* pref_name;
    switch (promo_type) {
      case SignInPromoType::kPassword:
        pref_name =
            base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
                ? prefs::
                      kPasswordSignInPromoShownCountPerProfileForLimitsExperiment
                : prefs::kPasswordSignInPromoShownCountPerProfile;
        break;
      case SignInPromoType::kAddress:
        pref_name =
            base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
                ? prefs::
                      kAddressSignInPromoShownCountPerProfileForLimitsExperiment
                : prefs::kAddressSignInPromoShownCountPerProfile;
        break;
      case SignInPromoType::kBookmark:
        if (!base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
          return;
        }
        pref_name =
            base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
                ? prefs::
                      kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment
                : prefs::kBookmarkSignInPromoShownCountPerProfile;
        break;
      case SignInPromoType::kExtension:
      case SignInPromoType::kSendTabToSelf:
        return;
    }

    int show_count = profile->GetPrefs()->GetInteger(pref_name);
    profile->GetPrefs()->SetInteger(pref_name, show_count + 1);
    return;
  }

  // Record the pref for the account that was used for the promo, either because
  // it is signed into the web or in sign in pending state.
  switch (promo_type) {
    case SignInPromoType::kPassword:
      SigninPrefs(*profile->GetPrefs())
          .IncrementPasswordSigninPromoImpressionCount(account.gaia);
      return;
    case SignInPromoType::kAddress:
      SigninPrefs(*profile->GetPrefs())
          .IncrementAddressSigninPromoImpressionCount(account.gaia);
      return;
    case SignInPromoType::kBookmark:
      if (base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
        SigninPrefs(*profile->GetPrefs())
            .IncrementBookmarkSigninPromoImpressionCount(account.gaia);
      }
      return;
    case SignInPromoType::kExtension:
    case SignInPromoType::kSendTabToSelf:
      return;
  }
}

bool ShouldUseAutofillSignInPromoLimits(signin::SignInPromoType promo_type) {
  return !base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment);
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
