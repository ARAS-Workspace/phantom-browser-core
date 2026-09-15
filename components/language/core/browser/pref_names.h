// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_

#include "build/build_config.h"

namespace language::prefs {

// The value to use for Accept-Languages HTTP header when making an HTTP
// request. This should not be set directly as it is a combination of
// kSelectedLanguages and kForcedLanguages. To update the list of preferred
// languages, set kSelectedLanguages and this pref will update automatically.
inline constexpr char kAcceptLanguages[] = "intl.accept_languages";

// List which contains the user-selected languages.
inline constexpr char kSelectedLanguages[] = "intl.selected_languages";

// List which contains the policy-forced languages.
inline constexpr char kForcedLanguages[] = "intl.forced_languages";

// The application locale as selected by the user, such as "en-AU". This may not
// necessarily be a string locale (a locale that we have strings for on this
// platform). Use |l10n_util::CheckAndResolveLocale| to convert it to a string
// locale if needed, such as "en-GB".
inline constexpr char kApplicationLocale[] = "intl.app_locale";

#if BUILDFLAG(IS_ANDROID)
inline constexpr char kAppLanguagePromptShown[] =
    "language.app_language_prompt_shown";

inline constexpr char kULPLanguages[] = "language.ulp_languages";
#endif

// Boolean that is true when offering translate (i.e. the automatic Full Page
// Translate bubble) is enabled. Even when this is false, the user can force
// translate from the right-click context menu unless translate is disabled by
// policy.
inline constexpr char kOfferTranslateEnabled[] = "translate.enabled";
inline constexpr char kPrefAlwaysTranslateList[] = "translate_allowlists";
inline constexpr char kPrefTranslateRecentTarget[] = "translate_recent_target";
inline constexpr char kPrefTranslateRecentTargets[] =
    "translate_recent_targets";
// Languages that the user marked as "do not translate".
inline constexpr char kBlockedLanguages[] = "translate_blocked_languages";
// Sites that never prompt to translate.
inline constexpr char kPrefNeverPromptSitesWithTime[] =
    "translate_site_blocklist_with_time";

// LINT.IfChange(DataRegion)
// The data region setting. 0: Unset, 1: US, 2: EU.
// Same value as chrome::prefs::kChromeDataRegionSetting.
inline constexpr char kTranslateDataRegionSetting[] =
    "chrome_data_region_setting";
// LINT.ThenChange(//components/translate/core/browser/translate_prefs.h:DataRegion)

}  // namespace language::prefs

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_
