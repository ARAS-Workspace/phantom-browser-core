// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_ITEM_MODE_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_ITEM_MODE_H_

class DownloadUIModel;

namespace download {

// Security UI mode of a download item.
enum class DownloadItemMode {
  kNormal,                 // Showing download item.
  kInsecureDownloadWarn,   // Displaying the insecure download warning.
  kInsecureDownloadBlock,  // Displaying the insecure download block error.
};

// Returns the mode that best reflects the current model state.
DownloadItemMode GetDesiredDownloadItemMode(const DownloadUIModel* download);

}  // namespace download

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_ITEM_MODE_H_
