// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_BOOKMARK_UTIL_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_BOOKMARK_UTIL_H_

#include "components/user_data_importer/common/imported_bookmark_entry.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace user_data_importer {

// Imports bookmarks into the provided BookmarkModel.
// Returns the number of imported bookmarks.
size_t ImportBookmarks(bookmarks::BookmarkModel* bookmark_model,
                       std::vector<ImportedBookmarkEntry> bookmarks,
                       const std::u16string& import_folder_title);

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_UTILITY_BOOKMARK_UTIL_H_
