// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/base_paths_android.h"
#endif

namespace {

// Helper function to make the IsAccessAllowed test concise.
bool IsAccessAllowed(const std::string& path,
                     const std::string& profile_path) {
  return ChromeNetworkDelegate::IsAccessAllowed(
      base::FilePath::FromUTF8Unsafe(path),
      base::FilePath::FromUTF8Unsafe(profile_path));
}

}  // namespace

TEST(ChromeNetworkDelegateStaticTest, IsAccessAllowed) {
#if BUILDFLAG(IS_ANDROID)
  // Chrome OS and Android don't have access to random files.
  EXPECT_FALSE(IsAccessAllowed("/", ""));
  EXPECT_FALSE(IsAccessAllowed("/foo.txt", ""));
  // Empty path should not be allowed.
  EXPECT_FALSE(IsAccessAllowed("", ""));
#else
  // Platforms other than Chrome OS and Android have access to any files.
  EXPECT_TRUE(IsAccessAllowed("/", ""));
  EXPECT_TRUE(IsAccessAllowed("/foo.txt", ""));
#endif

#if BUILDFLAG(IS_ANDROID)
  // Android allows the following directories.
  EXPECT_TRUE(IsAccessAllowed("/sdcard", ""));
  EXPECT_TRUE(IsAccessAllowed("/mnt/sdcard", ""));
  // Files under the directories are allowed.
  EXPECT_TRUE(IsAccessAllowed("/sdcard/foo.txt", ""));
  // Make sure similar paths are not allowed.
  EXPECT_FALSE(IsAccessAllowed("/mnt/sdcard.txt", ""));
  EXPECT_FALSE(IsAccessAllowed("/mnt", ""));

  // Files in external storage are allowed.
  base::FilePath external_storage_path;
  base::PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE,
                         &external_storage_path);
  EXPECT_TRUE(IsAccessAllowed(
      external_storage_path.AppendASCII("foo.txt").AsUTF8Unsafe(), ""));
  // The external storage root itself is not allowed.
  EXPECT_FALSE(IsAccessAllowed(external_storage_path.AsUTF8Unsafe(), ""));
#endif
}
