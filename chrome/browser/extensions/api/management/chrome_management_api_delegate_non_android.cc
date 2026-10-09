// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"
#include "chrome/browser/extensions/chrome_app_deprecation.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "components/services/app_service/public/cpp/app_launch_params.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/management.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/mojom/context_type.mojom.h"

namespace extensions {
bool ChromeManagementAPIDelegate::LaunchAppFunctionDelegate(
    const Extension* extension,
    content::BrowserContext* context) const {
  // Look at prefs to find the right launch container.
  // If the user has not set a preference, the default launch value will be
  // returned.
  // TODO(crbug.com/40098656): Make AppLaunchParams launch container Optional or
  // add a "default" launch container enum value.
  apps::LaunchContainer launch_container =
      GetLaunchContainer(ExtensionPrefs::Get(context), extension);
  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (IsExtensionUnsupportedDeprecatedApp(profile, extension->id())) {
    return false;
  }
#endif
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    // If the profile doesn't have an App Service Proxy available, that means
    // this extension has been explicitly permitted to run in an incognito
    // context. Treat this as if the extension is running in the original
    // profile, so it is allowed to access apps in the original profile.
    profile = profile->GetOriginalProfile();
  }
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithParams(
      apps::AppLaunchParams(extension->id(), launch_container,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB,
                            apps::LaunchSource::kFromManagementApi));

  RecordAppLaunchType(extension_misc::APP_LAUNCH_EXTENSION_API,
                      extension->GetType());
  return true;
}

bool ChromeManagementAPIDelegate::CreateAppShortcutFunctionDelegate(
    ManagementCreateAppShortcutFunction* function,
    const Extension* extension,
    std::string* error) const {
  BrowserWindowInterface* browser =
      ProfileBrowserCollection::GetForProfile(
          Profile::FromBrowserContext(function->browser_context()))
          ->GetLastActiveBrowser();
  if (!browser) {
    // Shouldn't happen if we have user gesture.
    *error = extension_management_api_constants::kNoBrowserToCreateShortcut;
    return false;
  }

  chrome::ShowCreateChromeAppShortcutsDialog(
      browser->GetWindow()->GetNativeWindow(), browser->GetProfile(), extension,
      base::BindOnce(
          &ManagementCreateAppShortcutFunction::OnCloseShortcutPrompt,
          function));

  return true;
}

}  // namespace extensions
