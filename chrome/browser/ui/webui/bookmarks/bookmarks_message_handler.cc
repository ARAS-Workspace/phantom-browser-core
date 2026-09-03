// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmarks_message_handler.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/gaia_id.h"

namespace {

}  // namespace

BookmarksMessageHandler::BookmarksMessageHandler() = default;

BookmarksMessageHandler::~BookmarksMessageHandler() = default;

void BookmarksMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getIncognitoAvailability",
      base::BindRepeating(
          &BookmarksMessageHandler::HandleGetIncognitoAvailability,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCanEditBookmarks",
      base::BindRepeating(&BookmarksMessageHandler::HandleGetCanEditBookmarks,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCanUploadBookmarkToAccountStorage",
      base::BindRepeating(
          &BookmarksMessageHandler::HandleGetCanUploadBookmarkToAccountStorage,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onSingleBookmarkUploadClicked",
      base::BindRepeating(&BookmarksMessageHandler::HandleSingleUploadClicked,
                          base::Unretained(this)));
}

void BookmarksMessageHandler::OnJavascriptAllowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  CHECK(!profile->IsGuestSession(),
        base::NotFatalUntil(base::NotFatalUntil::M140));
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      policy::policy_prefs::kIncognitoModeAvailability,
      base::BindRepeating(&BookmarksMessageHandler::UpdateIncognitoAvailability,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      bookmarks::prefs::kEditBookmarksEnabled,
      base::BindRepeating(&BookmarksMessageHandler::UpdateCanEditBookmarks,
                          base::Unretained(this)));

  // Sync Service is null in incognito mode.
  if (auto* sync_service = SyncServiceFactory::GetForProfile(profile)) {
    sync_service_observation_.Observe(sync_service);
  }
}

void BookmarksMessageHandler::OnJavascriptDisallowed() {
  pref_change_registrar_.RemoveAll();
  sync_service_observation_.Reset();
}

int BookmarksMessageHandler::GetIncognitoAvailability() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  return prefs->GetInteger(policy::policy_prefs::kIncognitoModeAvailability);
}

void BookmarksMessageHandler::HandleGetIncognitoAvailability(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();

  ResolveJavascriptCallback(callback_id,
                            base::Value(GetIncognitoAvailability()));
}

void BookmarksMessageHandler::UpdateIncognitoAvailability() {
  FireWebUIListener("incognito-availability-changed",
                    base::Value(GetIncognitoAvailability()));
}

bool BookmarksMessageHandler::CanEditBookmarks() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  return prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled);
}

void BookmarksMessageHandler::HandleGetCanEditBookmarks(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();

  ResolveJavascriptCallback(callback_id, base::Value(CanEditBookmarks()));
}

bool BookmarksMessageHandler::CanUploadBookmarkToAccountStorage(
    const std::string& id_string) {
  int64_t id;

  // Check if the bookmark's id is valid.
  if (!base::StringToInt64(id_string, &id)) {
    return false;
  }

  // Do not proceed if bookmarks cannot be edited.
  if (!CanEditBookmarks()) {
    return false;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  // Incognito profile should not show the upload button. The action is
  // possible, but it should not be promoted.
  if (profile->IsOffTheRecord()) {
    return false;
  }

  // Identity manager should always be valid since Incognito and Guest mode are
  // filtered out above.
  // Only signed in users may see the upload button.
  if (signin_util::GetSignedInState(IdentityManagerFactory::GetForProfile(
          profile)) != signin_util::SignedInState::kSignedIn) {
    return false;
  }

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, id);

  // Do not proceed if the bookmark does not exist.
  if (!node) {
    return false;
  }

  // Do not proceed if the node is a permanent node.
  if (model->is_permanent_node(node)) {
    return false;
  }

  // Do not proceed if the user is not using account storage.
  if (!model->account_other_node()) {
    return false;
  }

  // Do not proceed if the bookmark is managed.
  if (ManagedBookmarkServiceFactory::GetForProfile(profile)->IsNodeManaged(
          node)) {
    return false;
  }

  // Do not proceed if the bookmark is already in the account storage, or if the
  // user is syncing.
  if (!model->IsLocalOnlyNode(*node)) {
    return false;
  }

  return true;
}

void BookmarksMessageHandler::HandleGetCanUploadBookmarkToAccountStorage(
    const base::ListValue& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string& id = args[1].GetString();

  AllowJavascript();

  ResolveJavascriptCallback(callback_id,
                            base::Value(CanUploadBookmarkToAccountStorage(id)));
}

void BookmarksMessageHandler::HandleSingleUploadClicked(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  const std::string& id_string = args[0].GetString();
  int64_t id;
  base::StringToInt64(id_string, &id);

  Profile* profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);

  // Do not continue if account nodes are no longer available. This can happen
  // if the user signs out and the UI is not updated properly.
  // TODO(crbug.com/413637312): Remove this once the icon is no longer visible
  // upon sign out.
  if (!model->account_other_node()) {
    return;
  }

  // All conditions for uploading to account storage should be met at this
  // point.
  CHECK(CanUploadBookmarkToAccountStorage(id_string));

  // Show the dialog asking the user to confirm their choice to move the
  // bookmark.
  BrowserWindowInterface* const browser =
      ProfileBrowserCollection::GetForProfile(profile)->GetLastActiveBrowser();
  ShowBookmarkAccountStorageUploadDialog(
      browser, bookmarks::GetBookmarkNodeByID(model, id));
}

void BookmarksMessageHandler::UpdateCanEditBookmarks() {
  FireWebUIListener("can-edit-bookmarks-changed",
                    base::Value(CanEditBookmarks()));
}

void BookmarksMessageHandler::OnStateChanged(
    syncer::SyncService* sync_service) {
  if (sync_service->GetTransportState() !=
      syncer::SyncService::TransportState::CONFIGURING) {
    // Check if the bookmark sync state has changed.
    const bool new_active_state =
        sync_service->GetActiveDataTypes().Has(syncer::BOOKMARKS);
    if (is_bookmarks_sync_active_ != new_active_state) {
      is_bookmarks_sync_active_ = new_active_state;
      FireWebUIListener("bookmarks-sync-state-changed");
    }
  }
}

void BookmarksMessageHandler::OnSyncShutdown(
    syncer::SyncService* sync_service) {
  // Unreachable, since this class is tied to UI which gets destroyed before the
  // Profile and its KeyedServices.
  NOTREACHED();
}
