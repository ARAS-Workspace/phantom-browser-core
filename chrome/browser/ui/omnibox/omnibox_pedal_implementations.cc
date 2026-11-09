// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/resources/grit/omnibox_pedal_synonyms.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/app/vector_icons/vector_icons.h"
#endif


// =============================================================================

class OmniboxPedalClearBrowsingData : public OmniboxPedal {
 public:
  explicit OmniboxPedalClearBrowsingData(bool incognito)
      : OmniboxPedal(
            OmniboxPedalId::CLEAR_BROWSING_DATA,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
                IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA),
            GURL("chrome://settings/clearBrowserData")),
        incognito_(incognito) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    // Note about translation strings: English preserves original separated
    // synonym group structure, but these strings are not translated (grit
    // message sets translateable=false). Non-English uses simplified whole
    // phrase trigger lists, and this string is translated.
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_OPTIONAL_GOOGLE_CHROME,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_REQUIRED_DELETE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_REQUIRED_INFORMATION,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_REQUIRED_CLEAR_BROWSER_CACHE,
          },
      };
    }
  }

  void Execute(ExecutionContext& context) const override {
    if (incognito_) {
      context.client_->OpenIncognitoClearBrowsingDataDialog();
    } else {
      OmniboxPedal::Execute(context);
    }
  }

  // This method override enables this Pedal to spoof its ID for metrics
  // reporting, making it possible to distinguish incognito usage.
  OmniboxPedalId GetMetricsId() const override {
    return incognito_ ? OmniboxPedalId::INCOGNITO_CLEAR_BROWSING_DATA
                      : PedalId();
  }

 protected:
  ~OmniboxPedalClearBrowsingData() override = default;
  bool incognito_;
};

// =============================================================================

class OmniboxPedalLaunchIncognito : public OmniboxPedal {
 public:
  OmniboxPedalLaunchIncognito()
      : OmniboxPedal(
            OmniboxPedalId::LAUNCH_INCOGNITO,
            LabelStrings(IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_HINT,
                         IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_LAUNCH_INCOGNITO),
            GURL("chrome://newtab?incognito=true")) {
  }

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_LAUNCH_INCOGNITO_ONE_OPTIONAL_GOOGLE_CHROME,
          },
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_LAUNCH_INCOGNITO_ONE_OPTIONAL_CREATE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_LAUNCH_INCOGNITO_ONE_REQUIRED_INCOGNITO_WINDOW,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_LAUNCH_INCOGNITO_ONE_REQUIRED_ENTER_INCOGNITO_MODE,
          },
      };
    }
  }

  void Execute(ExecutionContext& context) const override {
    context.client_->NewIncognitoWindow();
  }
  bool IsReadyToTrigger(
      const AutocompleteInput& input,
      const AutocompleteProviderClient& client) const override {
    return client.IsIncognitoModeAvailable();
  }

 protected:
  ~OmniboxPedalLaunchIncognito() override = default;
};

// =============================================================================

class OmniboxPedalUpdateChrome : public OmniboxPedal {
 public:
  OmniboxPedalUpdateChrome()
      : OmniboxPedal(
            OmniboxPedalId::UPDATE_CHROME,
            LabelStrings(IDS_OMNIBOX_PEDAL_UPDATE_CHROME_HINT,
                         IDS_OMNIBOX_PEDAL_UPDATE_CHROME_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_UPDATE_CHROME_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_UPDATE_CHROME),
            GURL("chrome://settings/help")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CHROME_ONE_REQUIRED_GOOGLE_CHROME,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CHROME_ONE_REQUIRED_INSTALL,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_UPDATE_CHROME_ONE_REQUIRED_UPDATE_CHROME,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalUpdateChrome() override = default;
};

// =============================================================================

class OmniboxPedalManageSecuritySettings : public OmniboxPedal {
 public:
  OmniboxPedalManageSecuritySettings()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_SECURITY_SETTINGS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SECURITY_SETTINGS),
            GURL("chrome://settings/security")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              false,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SECURITY_SETTINGS_ANY_OPTIONAL_GOOGLE_CHROME,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SECURITY_SETTINGS_ONE_REQUIRED_ENHANCED_PROTECTION,
          },
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SECURITY_SETTINGS_ONE_OPTIONAL_ALTER,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SECURITY_SETTINGS_ONE_REQUIRED_MANAGE_SECURITY_SETTINGS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageSecuritySettings() override = default;
};

// =============================================================================

class OmniboxPedalManageCookies : public OmniboxPedal {
 public:
  OmniboxPedalManageCookies()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_COOKIES,
            LabelStrings(IDS_OMNIBOX_PEDAL_MANAGE_COOKIES_HINT,
                         IDS_OMNIBOX_PEDAL_MANAGE_COOKIES_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_COOKIES_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_MANAGE_COOKIES),
            GURL("chrome://settings/cookies")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_COOKIES_ONE_REQUIRED_COOKIE_SETTINGS,
          },
          {
              true,
              false,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_COOKIES_ANY_REQUIRED_GOOGLE_CHROME,
          },
          {
              false,
              false,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_COOKIES_ANY_OPTIONAL_THIRD_PARTY,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_COOKIES_ONE_REQUIRED_CHROME_COOKIE_SETTINGS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageCookies() override = default;
};

// =============================================================================

class OmniboxPedalManageSync : public OmniboxPedal {
 public:
  OmniboxPedalManageSync()
      : OmniboxPedal(OmniboxPedalId::MANAGE_SYNC,
                     GetLabelStrings(),
                     GURL("chrome://settings/syncSetup/advanced")) {}

  static LabelStrings GetLabelStrings() {
    if (base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
      return LabelStrings(
          IDS_OMNIBOX_PEDAL_MANAGE_SYNC_HINT_UPDATED,
          IDS_OMNIBOX_PEDAL_MANAGE_SYNC_SUGGESTION_CONTENTS_UPDATED,
          IDS_ACC_OMNIBOX_PEDAL_MANAGE_SYNC_SUFFIX_UPDATED,
          IDS_ACC_OMNIBOX_PEDAL_MANAGE_SYNC_UPDATED);
    }
    return LabelStrings(IDS_OMNIBOX_PEDAL_MANAGE_SYNC_HINT,
                        IDS_OMNIBOX_PEDAL_MANAGE_SYNC_SUGGESTION_CONTENTS,
                        IDS_ACC_OMNIBOX_PEDAL_MANAGE_SYNC_SUFFIX,
                        IDS_ACC_OMNIBOX_PEDAL_MANAGE_SYNC);
  }

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
      if (locale_is_english) {
        return {
            {
                true,
                true,
                IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SYNC_ONE_REQUIRED_SYNC_SETTINGS_UPDATED,
            },
            {
                true,
                false,
                IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SYNC_ANY_REQUIRED_GOOGLE_CHROME,
            },
        };
      } else {
        return {
            {
                true,
                true,
                IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SYNC_UPDATED,
            },
        };
      }
    }
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SYNC_ONE_REQUIRED_SYNC_SETTINGS,
          },
          {
              true,
              false,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SYNC_ANY_REQUIRED_GOOGLE_CHROME,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SYNC_ONE_REQUIRED_CHANGE_CHROME_BROWSER_SYNC_SETTINGS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageSync() override = default;
};

// =============================================================================

class OmniboxPedalManageSiteSettings : public OmniboxPedal {
 public:
  OmniboxPedalManageSiteSettings()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_SITE_SETTINGS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_SITE_SETTINGS),
            GURL(chrome::kChromeUIContentSettingsURL)) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SITE_SETTINGS_ONE_REQUIRED_SITE_PERMISSIONS,
          },
          {
              true,
              false,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SITE_SETTINGS_ANY_REQUIRED_GOOGLE_CHROME,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_SITE_SETTINGS_ONE_REQUIRED_ADJUST_SITE_PERMISSIONS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageSiteSettings() override = default;
};

// =============================================================================

class OmniboxPedalCloseIncognitoWindows : public OmniboxPedal {
 public:
  OmniboxPedalCloseIncognitoWindows()
      : OmniboxPedal(
            OmniboxPedalId::CLOSE_INCOGNITO_WINDOWS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CLOSE_INCOGNITO_WINDOWS_HINT,
                IDS_OMNIBOX_PEDAL_CLOSE_INCOGNITO_WINDOWS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CLOSE_INCOGNITO_WINDOWS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CLOSE_INCOGNITO_WINDOWS),
            GURL()) {}

  const gfx::VectorIcon& GetVectorIcon() const override {
    return features::IsRoundedIconsEnabled() ? omnibox::kIncognitoIcon
                                             : omnibox::kIncognitoCr2023OldIcon;
  }

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CLOSE_INCOGNITO_WINDOWS_ONE_REQUIRED_DELETE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CLOSE_INCOGNITO_WINDOWS_ONE_REQUIRED_INCOGNITO_WINDOW,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CLOSE_INCOGNITO_WINDOWS_ONE_REQUIRED_CLOSE_INCOGNITO_WINDOW,
          },
      };
    }
  }

  void Execute(ExecutionContext& context) const override {
    context.client_->CloseIncognitoWindows();
  }

 protected:
  ~OmniboxPedalCloseIncognitoWindows() override = default;
};

// =============================================================================

class OmniboxPedalPlayChromeDinoGame : public OmniboxPedal {
 public:
  OmniboxPedalPlayChromeDinoGame()
      : OmniboxPedal(
            OmniboxPedalId::PLAY_CHROME_DINO_GAME,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_PLAY_CHROME_DINO_GAME_HINT,
                IDS_OMNIBOX_PEDAL_PLAY_CHROME_DINO_GAME_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_PLAY_CHROME_DINO_GAME_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_PLAY_CHROME_DINO_GAME),
            GURL("chrome://dino")) {}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override {
    return features::IsRoundedIconsEnabled() ? omnibox::kOfflineDinoIcon
                                             : omnibox::kDinoCr2023OldIcon;
  }
#endif

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_PLAY_CHROME_DINO_GAME_ONE_REQUIRED_PLAY_CHROME_DINO_GAME,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_PLAY_CHROME_DINO_GAME_ONE_REQUIRED_CHROME_DINO,
          },
      };
    }
  }

  bool IsReadyToTrigger(
      const AutocompleteInput& input,
      const AutocompleteProviderClient& client) const override {
    return client.GetPrefs()->GetBoolean("allow_dinosaur_easter_egg");
  }

 protected:
  ~OmniboxPedalPlayChromeDinoGame() override = default;
};

// =============================================================================

class OmniboxPedalManageChromeSettings : public OmniboxPedal {
 public:
  OmniboxPedalManageChromeSettings()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_CHROME_SETTINGS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_SETTINGS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_SETTINGS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_SETTINGS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_SETTINGS),
            GURL("chrome://settings")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_SETTINGS_ONE_OPTIONAL_CONTROL,
          },
          {true, true,
           IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_SETTINGS_ONE_REQUIRED_CHROME_BROWSER_SETTINGS},
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_SETTINGS_ONE_REQUIRED_CHANGE_CHROME_SETTINGS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageChromeSettings() override = default;
};

// =============================================================================

class OmniboxPedalManageChromeDownloads : public OmniboxPedal {
 public:
  OmniboxPedalManageChromeDownloads()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_CHROME_DOWNLOADS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_DOWNLOADS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_DOWNLOADS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_DOWNLOADS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_DOWNLOADS),
            GURL("chrome://downloads")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_DOWNLOADS_ONE_REQUIRED_CONTROL,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_DOWNLOADS_ONE_REQUIRED_CHROME_BROWSER_DOWNLOADS,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_DOWNLOADS_ONE_REQUIRED_MANAGE_CHROME_DOWNLOADS,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageChromeDownloads() override = default;
};

// =============================================================================

class OmniboxPedalViewChromeHistory : public OmniboxPedal {
 public:
  OmniboxPedalViewChromeHistory()
      : OmniboxPedal(
            OmniboxPedalId::VIEW_CHROME_HISTORY,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_VIEW_CHROME_HISTORY_HINT,
                IDS_OMNIBOX_PEDAL_VIEW_CHROME_HISTORY_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_VIEW_CHROME_HISTORY_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_VIEW_CHROME_HISTORY),
            GURL("chrome://history")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_VIEW_CHROME_HISTORY_ONE_REQUIRED_REVISIT,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_VIEW_CHROME_HISTORY_ONE_REQUIRED_GOOGLE_CHROME_BROWSING_HISTORY,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_VIEW_CHROME_HISTORY_ONE_REQUIRED_SEE_CHROME_HISTORY,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalViewChromeHistory() override = default;
};

// =============================================================================

class OmniboxPedalShareThisPage : public OmniboxPedal {
 public:
  OmniboxPedalShareThisPage()
      : OmniboxPedal(
            OmniboxPedalId::SHARE_THIS_PAGE,
            LabelStrings(IDS_OMNIBOX_PEDAL_SHARE_THIS_PAGE_HINT,
                         IDS_OMNIBOX_PEDAL_SHARE_THIS_PAGE_SUGGESTION_CONTENTS,
                         IDS_ACC_OMNIBOX_PEDAL_SHARE_THIS_PAGE_SUFFIX,
                         IDS_ACC_OMNIBOX_PEDAL_SHARE_THIS_PAGE),
            GURL()) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SHARE_THIS_PAGE_ONE_REQUIRED_SHARE_LINK_WITH_QR_CODE,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SHARE_THIS_PAGE_ONE_REQUIRED_SHARE_THIS_PAGE,
          },
      };
    }
  }

  const gfx::VectorIcon& GetVectorIcon() const override {
    return GetSharingHubVectorIcon();
  }

  bool IsReadyToTrigger(
      const AutocompleteInput& input,
      const AutocompleteProviderClient& client) const override {
    return client.IsSharingHubAvailable();
  }

  void Execute(ExecutionContext& context) const override {
    context.client_->OpenSharingHub();
  }

 protected:
  ~OmniboxPedalShareThisPage() override = default;
};

// =============================================================================

class OmniboxPedalManageChromeAccessibility : public OmniboxPedal {
 public:
  OmniboxPedalManageChromeAccessibility()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_CHROME_ACCESSIBILITY,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_ACCESSIBILITY_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_CHROME_ACCESSIBILITY_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_ACCESSIBILITY_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROME_ACCESSIBILITY),
            GURL("chrome://settings/accessibility")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_ACCESSIBILITY_ONE_OPTIONAL_CUSTOMIZE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_ACCESSIBILITY_ONE_REQUIRED_ACCESSIBILITY_SETTINGS,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROME_ACCESSIBILITY_ONE_REQUIRED_CUSTOMIZE_CHROME_ACCESSIBILITY,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalManageChromeAccessibility() override = default;
};

// =============================================================================

class OmniboxPedalManageChromeOSAccessibility : public OmniboxPedal {
 public:
  OmniboxPedalManageChromeOSAccessibility()
      : OmniboxPedal(
            OmniboxPedalId::MANAGE_CHROMEOS_ACCESSIBILITY,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_CHROMEOS_ACCESSIBILITY_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_CHROMEOS_ACCESSIBILITY_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROMEOS_ACCESSIBILITY_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_MANAGE_CHROMEOS_ACCESSIBILITY),
            GURL("chrome://os-settings/osAccessibility")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROMEOS_ACCESSIBILITY_ONE_OPTIONAL_CUSTOMIZE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROMEOS_ACCESSIBILITY_ONE_REQUIRED_ACCESSIBILITY_SETTINGS,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_MANAGE_CHROMEOS_ACCESSIBILITY_ONE_REQUIRED_CUSTOMIZE_CHROMEOS_ACCESSIBILITY,
          },
      };
    }
  }

  OmniboxPedalId GetMetricsId() const override {
    return OmniboxPedalId::MANAGE_CHROME_ACCESSIBILITY;
  }

 protected:
  ~OmniboxPedalManageChromeOSAccessibility() override = default;
};

// =============================================================================

class OmniboxPedalCustomizeChromeFonts : public OmniboxPedal {
 public:
  OmniboxPedalCustomizeChromeFonts()
      : OmniboxPedal(
            OmniboxPedalId::CUSTOMIZE_CHROME_FONTS,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CUSTOMIZE_CHROME_FONTS_HINT,
                IDS_OMNIBOX_PEDAL_CUSTOMIZE_CHROME_FONTS_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CUSTOMIZE_CHROME_FONTS_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CUSTOMIZE_CHROME_FONTS),
            GURL("chrome://settings/fonts")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_CHROME_FONTS_ONE_OPTIONAL_CUSTOMIZE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_CHROME_FONTS_ONE_REQUIRED_GOOGLE_CHROME,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_CHROME_FONTS_ONE_REQUIRED_FONT_SIZING,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_CHROME_FONTS_ONE_REQUIRED_CHANGE_BROWSER_FONT,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalCustomizeChromeFonts() override = default;
};

// =============================================================================

class OmniboxPedalCustomizeSearchEngines : public OmniboxPedal {
 public:
  OmniboxPedalCustomizeSearchEngines()
      : OmniboxPedal(
            OmniboxPedalId::CUSTOMIZE_SEARCH_ENGINES,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CUSTOMIZE_SEARCH_ENGINES_HINT,
                IDS_OMNIBOX_PEDAL_CUSTOMIZE_SEARCH_ENGINES_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CUSTOMIZE_SEARCH_ENGINES_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CUSTOMIZE_SEARCH_ENGINES),
            GURL("chrome://settings/searchEngines")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_SEARCH_ENGINES_ONE_OPTIONAL_CUSTOMIZE,
          },
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_SEARCH_ENGINES_ONE_REQUIRED_CUSTOM_SEARCH_ENGINES,
          },
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_SEARCH_ENGINES_ONE_OPTIONAL_GOOGLE_CHROME,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_CUSTOMIZE_SEARCH_ENGINES_ONE_REQUIRED_ADD_CUSTOM_SEARCH,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalCustomizeSearchEngines() override = default;
};

// =============================================================================

class OmniboxPedalSetChromeAsDefaultBrowser : public OmniboxPedal {
 public:
  OmniboxPedalSetChromeAsDefaultBrowser()
      : OmniboxPedal(
            OmniboxPedalId::SET_CHROME_AS_DEFAULT_BROWSER,
            LabelStrings(
                IDS_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER_HINT,
                IDS_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER),
            GURL("chrome://settings/defaultBrowser")) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    if (locale_is_english) {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SET_CHROME_AS_DEFAULT_BROWSER_ONE_REQUIRED_HOW_TO_MAKE_CHROME_MY_DEFAULT_BROWSER,
          },
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SET_CHROME_AS_DEFAULT_BROWSER_ONE_OPTIONAL_SELECT,
          },
          {
              false,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SET_CHROME_AS_DEFAULT_BROWSER_ONE_OPTIONAL_DEFAULT_BROWSER,
          },
      };
    } else {
      return {
          {
              true,
              true,
              IDS_OMNIBOX_PEDAL_SYNONYMS_SET_CHROME_AS_DEFAULT_BROWSER_ONE_REQUIRED_ALWAYS_OPEN_LINKS_IN_CHROME,
          },
      };
    }
  }

 protected:
  ~OmniboxPedalSetChromeAsDefaultBrowser() override = default;
};

// =============================================================================

const gfx::VectorIcon& GetSharingHubVectorIcon() {
#if BUILDFLAG(IS_MAC)
  return features::IsRoundedIconsEnabled()
             ? omnibox::kIosShareIcon
             : omnibox::kShareMacChromeRefreshOldIcon;
#elif BUILDFLAG(IS_LINUX)
  return features::IsRoundedIconsEnabled()
             ? omnibox::kSendIcon
             : omnibox::kShareLinuxChromeRefreshOldIcon;
#else
  return features::IsRoundedIconsEnabled()
             ? omnibox::kShareIcon
             : omnibox::kShareChromeRefreshOldIcon;
#endif
}

std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>>
GetPedalImplementations(bool incognito, bool guest) {
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    const bool inserted =
        pedals
            .insert(
                std::make_pair(pedal->PedalId(), base::WrapRefCounted(pedal)))
            .second;
    DCHECK(inserted);
  };

  // Clear Browsing Data functionality is disabled in guest mode, so
  // the pedal for accessing it should not be included.
  if (!guest) {
    add(new OmniboxPedalClearBrowsingData(incognito));
  }
  add(new OmniboxPedalLaunchIncognito());
  add(new OmniboxPedalUpdateChrome());
  add(new OmniboxPedalManageSecuritySettings());
  add(new OmniboxPedalManageCookies());
  add(new OmniboxPedalManageSync());
  add(new OmniboxPedalManageSiteSettings());

  if (incognito) {
    add(new OmniboxPedalCloseIncognitoWindows());
  }
  add(new OmniboxPedalPlayChromeDinoGame());
  add(new OmniboxPedalManageChromeSettings());
  add(new OmniboxPedalManageChromeDownloads());
  add(new OmniboxPedalViewChromeHistory());
  add(new OmniboxPedalShareThisPage());
  add(new OmniboxPedalManageChromeAccessibility());
  add(new OmniboxPedalSetChromeAsDefaultBrowser());
  add(new OmniboxPedalCustomizeChromeFonts());
  add(new OmniboxPedalCustomizeSearchEngines());

  return pedals;
}
