// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"

namespace settings {


void AddSharedSyncPageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"syncDisabledByAdministrator",
       IDS_SIGNED_IN_WITH_SYNC_DISABLED_BY_POLICY},
      {"passphrasePlaceholder", IDS_SETTINGS_PASSPHRASE_PLACEHOLDER},
      {"existingPassphraseTitle", IDS_SETTINGS_EXISTING_PASSPHRASE_TITLE},
      {"submitPassphraseButton", IDS_SETTINGS_SUBMIT_PASSPHRASE},
      {"encryptionOptionsTitle", IDS_SETTINGS_ENCRYPTION_OPTIONS},
      {"mismatchedPassphraseError", IDS_SETTINGS_MISMATCHED_PASSPHRASE_ERROR},
      {"emptyPassphraseError", IDS_SETTINGS_EMPTY_PASSPHRASE_ERROR},
      {"incorrectPassphraseError", IDS_SETTINGS_INCORRECT_PASSPHRASE_ERROR},
      {"syncPageTitle", IDS_SETTINGS_SYNC_SYNC_AND_NON_PERSONALIZED_SERVICES},
      {"passphraseConfirmationPlaceholder",
       IDS_SETTINGS_PASSPHRASE_CONFIRMATION_PLACEHOLDER},
      {"syncLoading", IDS_SETTINGS_SYNC_LOADING},
      {"syncDataEncryptedText", IDS_SETTINGS_SYNC_DATA_ENCRYPTED_TEXT},
      {"sync", IDS_SETTINGS_SYNC},
      {"syncAdvancedBrowserPageTitle",
       IDS_SETTINGS_NEW_SYNC_ADVANCED_BROWSER_PAGE_TITLE},
      {"enterPassphraseLabel", IDS_SYNC_ENTER_PASSPHRASE_BODY},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  std::string sync_dashboard_url =
      google_util::AppendGoogleLocaleParam(
          GURL(base::FeatureList::IsEnabled(
                   syncer::kSyncEnableNewSyncDashboardUrl)
                   ? chrome::kNewSyncGoogleDashboardURL
                   : chrome::kLegacySyncGoogleDashboardURL),
          g_browser_process->GetApplicationLocale())
          .spec();

  html_source->AddString(
      "passphraseRecover",
      l10n_util::GetStringFUTF8(IDS_SETTINGS_PASSPHRASE_RECOVER,
                                base::ASCIIToUTF16(sync_dashboard_url)));

  const bool updateAccountSettingsStrings =
      syncer::IsReplaceSyncPromosWithSignInPromosEnabled();

  html_source->AddLocalizedString(
      "encryptWithGoogleCredentialsLabel",
      updateAccountSettingsStrings
          ? IDS_SETTINGS_ENCRYPT_ACCOUNT_DATA_WITH_GOOGLE_CREDENTIALS_LABEL
          : IDS_SETTINGS_ENCRYPT_WITH_GOOGLE_CREDENTIALS_LABEL);
  html_source->AddLocalizedString(
      "existingPassphraseLabelWithDate",
      updateAccountSettingsStrings
          ? IDS_SYNC_FULL_ACCOUNT_DATA_ENCRYPTION_BODY_CUSTOM_WITH_DATE
          : IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM_WITH_DATE);
  html_source->AddLocalizedString(
      "existingPassphraseLabel",
      updateAccountSettingsStrings
          ? IDS_SYNC_FULL_ACCOUNT_DATA_ENCRYPTION_BODY_CUSTOM
          : IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM);

  html_source->AddString(
      "passphraseResetHintEncryption",
      l10n_util::GetStringFUTF8(
          updateAccountSettingsStrings
              ? IDS_SETTINGS_PASSPHRASE_RESET_HINT_ACCOUNT_DATA_ENCRYPTION
              : IDS_SETTINGS_PASSPHRASE_RESET_HINT_ENCRYPTION,
          base::ASCIIToUTF16(sync_dashboard_url)));
  html_source->AddString(
      "passphraseExplanationText",
      l10n_util::GetStringFUTF8(
          updateAccountSettingsStrings
              ? IDS_SETTINGS_ENCRYPT_ACCOUNT_DATA_WITH_PASSPHRASE_EXPLANATION_TEXT
              : IDS_SETTINGS_PASSPHRASE_EXPLANATION_TEXT,
          base::ASCIIToUTF16(sync_dashboard_url)));
  html_source->AddString(
      "encryptWithSyncPassphraseLabel",
      l10n_util::GetStringFUTF8(
          updateAccountSettingsStrings
              ? IDS_SETTINGS_ENCRYPT_ACCOUNT_DATA_WITH_PASSPHRASE_LABEL
              : IDS_SETTINGS_ENCRYPT_WITH_SYNC_PASSPHRASE_LABEL,
          chrome::kSyncEncryptionHelpURL));
}

void AddSecureDnsStrings(content::WebUIDataSource* html_source) {
  webui::LocalizedString kLocalizedStrings[] = {
      {"secureDns", IDS_SETTINGS_SECURE_DNS},
      {"secureDnsDescription", IDS_SETTINGS_SECURE_DNS_DESCRIPTION},
      {"secureDnsDisabledForManagedEnvironment",
       IDS_SETTINGS_SECURE_DNS_DISABLED_FOR_MANAGED_ENVIRONMENT},
      {"secureDnsDisabledForParentalControl",
       IDS_SETTINGS_SECURE_DNS_DISABLED_FOR_PARENTAL_CONTROL},
      {"secureDnsAutomaticModeDescription",
       IDS_SETTINGS_AUTOMATIC_MODE_DESCRIPTION},
      {"secureDnsCustomProviderDescription",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_DESCRIPTION},
      {"secureDnsDropdownA11yLabel",
       IDS_SETTINGS_SECURE_DNS_DROPDOWN_ACCESSIBILITY_LABEL},
      {"secureDnsSecureDropdownModeDescription",
       IDS_SETTINGS_SECURE_DROPDOWN_MODE_DESCRIPTION},
      {"secureDnsSecureDropdownModePrivacyPolicy",
       IDS_SETTINGS_SECURE_DROPDOWN_MODE_PRIVACY_POLICY},
      {"secureDnsCustomPlaceholder",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_PLACEHOLDER},
      {"secureDnsCustomFormatError",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_FORMAT_ERROR},
      {"secureDnsCustomConnectionError",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_CONNECTION_ERROR},
  };

  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace settings
