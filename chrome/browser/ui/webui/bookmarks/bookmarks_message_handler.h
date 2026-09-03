// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_MESSAGE_HANDLER_H_

#include "base/scoped_observation.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

class BookmarksMessageHandler : public content::WebUIMessageHandler,
                                public syncer::SyncServiceObserver {
 public:
  BookmarksMessageHandler();

  BookmarksMessageHandler(const BookmarksMessageHandler&) = delete;
  BookmarksMessageHandler& operator=(const BookmarksMessageHandler&) = delete;

  ~BookmarksMessageHandler() override;

 private:
  friend class BookmarkMessageHandlerTest;

  int GetIncognitoAvailability();
  void HandleGetIncognitoAvailability(const base::ListValue& args);
  void UpdateIncognitoAvailability();

  bool CanEditBookmarks();
  void HandleGetCanEditBookmarks(const base::ListValue& args);
  void UpdateCanEditBookmarks();

  bool CanUploadBookmarkToAccountStorage(const std::string& id);
  void HandleGetCanUploadBookmarkToAccountStorage(const base::ListValue& args);
  void HandleSingleUploadClicked(const base::ListValue& args);

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

  // Keep track of the previous bookmarks sync state to filter out irrelevant
  // updates coming from `SyncService`.
  bool is_bookmarks_sync_active_ = false;

  PrefChangeRegistrar pref_change_registrar_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_MESSAGE_HANDLER_H_
