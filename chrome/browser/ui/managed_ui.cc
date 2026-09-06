// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include <optional>

#include "build/build_config.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/browser_management/management_identity.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/management/management_service.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "ui/chromeos/devicetype_utils.h"
#include "url/gurl.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
#include "components/strings/grit/components_strings.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
enum ManagementStringType : size_t {
  BROWSER_MANAGED = 0,
  BROWSER_MANAGED_BY = 1,
  BROWSER_PROFILE_SAME_MANAGED_BY = 2,
  BROWSER_PROFILE_DIFFERENT_MANAGED_BY = 3,
  BROWSER_MANAGED_PROFILE_MANAGED_BY = 4,
  PROFILE_MANAGED_BY = 5,
  SUPERVISED = 6,
  NOT_MANAGED = 7
};
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

bool ShouldDisplayManagedByParentUi(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  // Don't display the managed by parent UI on ChromeOS, because similar UI is
  // displayed at the OS level.
  return false;
#else
  return profile && profile->IsChild();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
ManagementStringType GetManagementStringType(Profile* profile) {
  if (!enterprise_util::IsBrowserManaged(profile) &&
      ShouldDisplayManagedByParentUi(profile)) {
    return SUPERVISED;
  }

  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();
  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);
  bool account_managed = management_service->IsAccountManaged();
  bool device_managed = management_service->IsBrowserManaged();
  bool known_device_manager = device_manager && !device_manager->empty();
  bool known_account_manager = account_manager && !account_manager->empty();

  // TODO (crbug://1227786) Add a PROFILE_MANAGED case, and ensure the following
  // tests are setup so that we do not have a managed account without an account
  // manager:  WebKioskTest.CloseSettingWindowIfOnlyOpen,
  // WebKioskTest.NotExitIfCloseSettingsWindow, WebKioskTest.OpenA11ySettings.
  if (account_managed && !known_account_manager) {
    account_managed = false;
  }

  if (!account_managed && !device_managed) {
    return NOT_MANAGED;
  }

  if (!device_managed) {
    return known_account_manager ? PROFILE_MANAGED_BY : BROWSER_MANAGED;
  }

  if (!account_managed) {
    return known_device_manager ? BROWSER_MANAGED_BY : BROWSER_MANAGED;
  }

  CHECK(known_account_manager);
  if (known_device_manager) {
    return *account_manager == *device_manager
               ? BROWSER_PROFILE_SAME_MANAGED_BY
               : BROWSER_PROFILE_DIFFERENT_MANAGED_BY;
  }

  return BROWSER_MANAGED_PROFILE_MANAGED_BY;
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

}  // namespace

bool ShouldDisplayManagedUi(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  // Don't show the UI in demo mode.
  if (ash::demo_mode::IsDeviceInDemoMode()) {
    return false;
  }

  // Don't show the UI for Family Link accounts.
  if (profile->IsChild()) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return enterprise_util::IsBrowserManaged(profile) ||
         ShouldDisplayManagedByParentUi(profile);
}

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS_CORE)
const gfx::VectorIcon& GetManagedUiIcon(Profile* profile) {
  CHECK(ShouldDisplayManagedUi(profile));

  if (enterprise_util::IsBrowserManaged(profile)) {
    return features::IsRoundedIconsEnabled()
               ? vector_icons::kDomainIcon
               : vector_icons::kBusinessChromeRefreshOldIcon;
  }

  CHECK(ShouldDisplayManagedByParentUi(profile));
  return features::IsRoundedIconsEnabled() ? vector_icons::kFamilyLinkIcon
                                           : vector_icons::kFamilyLinkOldIcon;
}
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(IS_CHROMEOS)
GURL GetManagedUiUrl(Profile* profile) {
  if (enterprise_util::IsBrowserManaged(profile)) {
    return GURL(chrome::kChromeUIManagementURL);
  }

  if (ShouldDisplayManagedByParentUi(profile)) {
    return GURL(supervised_user::kManagedByParentUiMoreInfoUrl);
  }

  return GURL();
}

std::string GetManagedUiWebUIIcon(Profile* profile) {
  if (enterprise_util::IsBrowserManaged(profile)) {
    return "cr:domain";
  }

  if (ShouldDisplayManagedByParentUi(profile)) {
    // The Family Link "kite" icon.
    return "cr20:family-link";
  }

  // This method can be called even if we shouldn't display the managed UI.
  return std::string();
}

std::u16string GetManagedUiWebUILabel(Profile* profile) {
  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();

  switch (GetManagementStringType(profile)) {
    case BROWSER_MANAGED:
      return l10n_util::GetStringFUTF16(IDS_MANAGED_WITH_HYPERLINK,
                                        chrome::kChromeUIManagementURL16);
    case BROWSER_MANAGED_BY:
      return l10n_util::GetStringFUTF16(IDS_MANAGED_BY_WITH_HYPERLINK,
                                        chrome::kChromeUIManagementURL16,
                                        base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_SAME_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_AND_PROFILE_SAME_MANAGED_BY_WITH_HYPERLINK,
          chrome::kChromeUIManagementURL16, base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_DIFFERENT_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY_WITH_HYPERLINK,
          chrome::kChromeUIManagementURL16, base::UTF8ToUTF16(*device_manager),
          base::UTF8ToUTF16(*account_manager));
    case BROWSER_MANAGED_PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_MANAGED_AND_PROFILE_MANAGED_BY_WITH_HYPERLINK,
          chrome::kChromeUIManagementURL16,
          base::UTF8ToUTF16(*account_manager));
    case PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(IDS_PROFILE_MANAGED_BY_WITH_HYPERLINK,
                                        chrome::kChromeUIManagementURL16,
                                        base::UTF8ToUTF16(*account_manager));
    case SUPERVISED:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGED_BY_PARENT_WITH_HYPERLINK,
          base::UTF8ToUTF16(supervised_user::kManagedByParentUiMoreInfoUrl));
    case NOT_MANAGED:
      return std::u16string();
  }
  return std::u16string();
}

std::u16string GetDeviceManagedUiWebUILabel() {
  int string_id = IDS_DEVICE_MANAGED_WITH_HYPERLINK;
  std::vector<std::u16string> replacements;
  replacements.push_back(chrome::kChromeUIManagementURL16);
  replacements.push_back(ui::GetChromeOSDeviceName());

  const std::optional<std::string> device_manager = GetDeviceManagerIdentity();
  if (device_manager && !device_manager->empty()) {
    string_id = IDS_DEVICE_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(*device_manager));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}
#elif BUILDFLAG(IS_ANDROID)
std::u16string GetManagementPageSubtitle(Profile* profile) {
  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();

  switch (GetManagementStringType(profile)) {
    case BROWSER_MANAGED:
      return l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE);
    case BROWSER_MANAGED_BY:
      return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                        base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_SAME_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_SAME_MANAGED_BY,
          base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_DIFFERENT_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY,
          base::UTF8ToUTF16(*device_manager),
          base::UTF8ToUTF16(*account_manager));
    case BROWSER_MANAGED_PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_MANAGED_AND_PROFILE_MANAGED_BY,
          base::UTF8ToUTF16(*account_manager));
    case PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_PROFILE_MANAGED_BY,
          base::UTF8ToUTF16(*account_manager));
    case SUPERVISED:
      return l10n_util::GetStringUTF16(IDS_MANAGED_BY_PARENT);
    case NOT_MANAGED:
      return l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE);
  }
  return std::u16string();
}
#endif
