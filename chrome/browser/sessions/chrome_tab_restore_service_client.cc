// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_common_utils.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/tab_groups/tab_group_id.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/mojom/window_show_state.mojom.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/apps/platform_apps/platform_app_launch.h"
#endif

#include "chrome/browser/ui/browser_live_tab_context.h"

ChromeTabRestoreServiceClient::ChromeTabRestoreServiceClient(Profile* profile)
    : profile_(profile) {}

ChromeTabRestoreServiceClient::~ChromeTabRestoreServiceClient() = default;

sessions::LiveTabContext* ChromeTabRestoreServiceClient::CreateLiveTabContext(
    sessions::LiveTabContext* existing_context,
    sessions::SessionWindow::WindowType type,
    const std::string& app_name,
    const gfx::Rect& bounds,
    ui::mojom::WindowShowState show_state,
    const std::string& workspace,
    const std::string& user_title,
    const std::map<std::string, std::string>& extra_data) {
  return BrowserLiveTabContext::Create(profile_, type, app_name, bounds,
                                       show_state, workspace, user_title,
                                       extra_data);
}

sessions::LiveTabContext*
ChromeTabRestoreServiceClient::FindLiveTabContextForTab(
    const sessions::LiveTab* tab) {
  return BrowserLiveTabContext::FindContextForWebContents(
      &static_cast<const sessions::ContentLiveTab*>(tab)->GetWebContents());
}

sessions::LiveTabContext*
ChromeTabRestoreServiceClient::FindLiveTabContextWithID(SessionID desired_id) {
  return BrowserLiveTabContext::FindContextWithID(desired_id);
}

sessions::LiveTabContext*
ChromeTabRestoreServiceClient::FindLiveTabContextWithGroup(
    tab_groups::TabGroupId group) {
  return BrowserLiveTabContext::FindContextWithGroup(group, profile_);
}

bool ChromeTabRestoreServiceClient::ShouldTrackURLForRestore(const GURL& url) {
  return ::ShouldTrackURLForRestore(url);
}

std::string ChromeTabRestoreServiceClient::GetExtensionAppIDForTab(
    sessions::LiveTab* tab) {
  std::string app_id;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  app_id = apps::GetAppIdForWebContents(
      &static_cast<sessions::ContentLiveTab*>(tab)->GetWebContents());
#endif

  return app_id;
}

base::FilePath ChromeTabRestoreServiceClient::GetPathToSaveTo() {
  return profile_->GetPath();
}

GURL ChromeTabRestoreServiceClient::GetNewTabURL() {
  return chrome::ChromeUINewTabURLAsGURL();
}

bool ChromeTabRestoreServiceClient::HasLastSession() {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  ExitType exit_type = ExitTypeService::GetLastSessionExitType(profile_);
  // The previous session crashed and wasn't restored, or was a forced
  // shutdown. Both of which won't have notified us of the browser close so
  // that we need to load the windows from session service (which will have
  // saved them).
  return (!profile_->restored_last_session() && session_service &&
          (exit_type == ExitType::kCrashed ||
           exit_type == ExitType::kForcedShutdown));
#else
  return false;
#endif
}

void ChromeTabRestoreServiceClient::GetLastSession(
    sessions::GetLastSessionCallback callback) {
  DCHECK(HasLastSession());
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  SessionServiceFactory::GetForProfile(profile_)->GetLastSession(
      std::move(callback));
#endif
}

void ChromeTabRestoreServiceClient::OnTabRestored(const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  apps::RecordExtensionAppLaunchOnTabRestored(profile_, url);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}
