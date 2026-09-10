// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "base/android/path_utils.h"
#endif

namespace {

bool g_access_to_all_files_enabled = false;

#if BUILDFLAG(IS_ANDROID)
// Returns true if |allowlist| contains |path| or a parent of |path|.
bool IsPathOnAllowlist(const base::FilePath& path,
                       const std::vector<base::FilePath>& allowlist) {
  for (const auto& allowlisted_path : allowlist) {
    // base::FilePath::operator== should probably handle trailing separators.
    if (allowlisted_path == path.StripTrailingSeparators() ||
        allowlisted_path.IsParent(path)) {
      return true;
    }
  }
  return false;
}
#endif

#if BUILDFLAG(IS_ANDROID)
// Returns true if access is allowed for |path|.
bool IsAccessAllowedAndroid(const base::FilePath& path) {
  // Access to files in external storage is allowed.
  base::FilePath external_storage_path;
  base::PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE,
                         &external_storage_path);
  if (external_storage_path.IsParent(path))
    return true;

  std::vector<base::FilePath> allowlist;
  std::vector<base::FilePath> all_download_dirs =
      base::android::GetAllPrivateDownloadsDirectories();
  allowlist.insert(allowlist.end(), all_download_dirs.begin(),
                   all_download_dirs.end());

  if (base::android::android_info::sdk_int() >
      base::android::android_info::SDK_VERSION_Q) {
    std::vector<base::FilePath> all_external_download_volumes =
        base::android::GetSecondaryStorageDownloadDirectories();
    allowlist.insert(allowlist.end(), all_external_download_volumes.begin(),
                     all_external_download_volumes.end());
  }

  // allowlist of other allowed directories.
  static const base::FilePath::CharType* const kLocalAccessAllowList[] = {
      "/sdcard",
      "/mnt/sdcard",
  };
  for (const auto* allowlisted_path : kLocalAccessAllowList)
    allowlist.emplace_back(allowlisted_path);

  return IsPathOnAllowlist(path, allowlist);
}
#endif  // BUILDFLAG(IS_ANDROID)

bool IsAccessAllowedInternal(const base::FilePath& path,
                             const base::FilePath& profile_path) {
  if (g_access_to_all_files_enabled)
    return true;

#if BUILDFLAG(IS_ANDROID)
  return IsAccessAllowedAndroid(path);
#else
  return true;
#endif
}

}  // namespace

// static
bool ChromeNetworkDelegate::IsAccessAllowed(
    const base::FilePath& path,
    const base::FilePath& profile_path) {
  return IsAccessAllowedInternal(path, profile_path);
}

// static
bool ChromeNetworkDelegate::IsAccessAllowed(
    const base::FilePath& path,
    const base::FilePath& absolute_path,
    const base::FilePath& profile_path) {
#if BUILDFLAG(IS_ANDROID)
  // Android's allowlist relies on symbolic links (ex. /sdcard is allowed
  // and commonly a symbolic link), thus do not check absolute paths.
  return IsAccessAllowedInternal(path, profile_path);
#else
  return (IsAccessAllowedInternal(path, profile_path) &&
          IsAccessAllowedInternal(absolute_path, profile_path));
#endif
}

// static
void ChromeNetworkDelegate::EnableAccessToAllFilesForTesting(bool enabled) {
  g_access_to_all_files_enabled = enabled;
}
