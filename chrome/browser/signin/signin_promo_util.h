// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/signin/signin_promo.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/buildflags/buildflags.h"

class Profile;

namespace signin_metrics {
enum class AccessPoint;
}

namespace autofill {
class AutofillProfile;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace extensions {
class Extension;
}

class PrefService;

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace signin {

enum class SignInPromoType;

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Whether we should show the sign in promo after an extension was installed.
bool ShouldShowExtensionSignInPromo(Profile& profile,
                                    const extensions::Extension& extension);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Whether we should show the sign in promo after a password was saved.
bool ShouldShowPasswordSignInPromo(Profile& profile);

// Whether we should show the sign in promo after `address` was saved.
bool ShouldShowAddressSignInPromo(Profile& profile,
                                  const autofill::AutofillProfile& address);

// Whether we should show the sign in promo after a bookmark was saved.
bool ShouldShowBookmarkSignInPromo(Profile& profile);

// Returns whether `access_point` has an equivalent signin promo which is its
// own bubble, rather than a footnote.
bool IsBubbleSigninPromo(signin_metrics::AccessPoint access_point);

// Returns whether `access_point` has an equivalent signin promo.
bool IsSignInPromo(signin_metrics::AccessPoint access_point);

SignInPromoType GetSignInPromoTypeFromAccessPoint(
    signin_metrics::AccessPoint access_point);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Records that the sign in promo was shown, either for the account used for the
// promo, or for the profile if there is no account available.
void RecordSignInPromoShown(signin_metrics::AccessPoint access_point,
                            Profile* profile);

// Returns true if the sign-in promo for `promo_type` should use the legacy
// global Autofill sign-in promo limits. This is true when the limits
// experiment is disabled and the promo type is not Search AI Mode.
bool ShouldUseAutofillSignInPromoLimits(signin::SignInPromoType promo_type);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
