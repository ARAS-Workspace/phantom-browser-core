// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/platform_util_internal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace platform_util {

namespace {

// Test fixture used by all desktop platforms other than Chrome OS.
class PlatformUtilTestBase : public InProcessBrowserTest {
 protected:
  Profile* GetProfile() { return nullptr; }
  void SetUpPlatformFixture(const base::FilePath&) {}
};

class PlatformUtilTest : public PlatformUtilTestBase {
 public:
  void SetUpOnMainThread() override {
    PlatformUtilTestBase::SetUpOnMainThread();

    static const char kTestFileData[] = "Cow says moo!";

    // This prevents platform_util from invoking any shell or external APIs
    // during tests. Doing so may result in external applications being launched
    // and intefering with tests.
    internal::DisableShellOperationsForTesting();

    ASSERT_TRUE(directory_.CreateUniqueTempDir());

    // A valid file.
    existing_file_ = directory_.GetPath().AppendASCII("test_file.txt");
    ASSERT_TRUE(base::WriteFile(existing_file_, kTestFileData));

    // A valid folder.
    existing_folder_ = directory_.GetPath().AppendASCII("test_folder");
    ASSERT_TRUE(base::CreateDirectory(existing_folder_));

    // A non-existent path.
    nowhere_ = directory_.GetPath().AppendASCII("nowhere");

    SetUpPlatformFixture(directory_.GetPath());
  }

  OpenOperationResult CallOpenItem(const base::FilePath& path,
                                   OpenItemType item_type) {
    base::RunLoop run_loop;
    OpenOperationResult result = OPEN_SUCCEEDED;
    OpenOperationCallback callback =
        base::BindOnce(&OnOpenOperationDone, run_loop.QuitClosure(), &result);
    OpenItem(GetProfile(), path, item_type, std::move(callback));
    run_loop.Run();
    return result;
  }

  base::FilePath existing_file_;
  base::FilePath existing_folder_;
  base::FilePath nowhere_;

 protected:
  base::ScopedTempDir directory_;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

  static void OnOpenOperationDone(base::OnceClosure closure,
                                  OpenOperationResult* store_result,
                                  OpenOperationResult result) {
    *store_result = result;
    std::move(closure).Run();
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PlatformUtilTest, OpenFile) {
  EXPECT_EQ(OPEN_SUCCEEDED, CallOpenItem(existing_file_, OPEN_FILE));
  EXPECT_EQ(OPEN_FAILED_INVALID_TYPE,
            CallOpenItem(existing_folder_, OPEN_FILE));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND, CallOpenItem(nowhere_, OPEN_FILE));
}

IN_PROC_BROWSER_TEST_F(PlatformUtilTest, OpenFolder) {
  EXPECT_EQ(OPEN_SUCCEEDED, CallOpenItem(existing_folder_, OPEN_FOLDER));
  EXPECT_EQ(OPEN_FAILED_INVALID_TYPE,
            CallOpenItem(existing_file_, OPEN_FOLDER));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND, CallOpenItem(nowhere_, OPEN_FOLDER));
}

#if BUILDFLAG(IS_POSIX)
// Symbolic links are currently only supported on Posix. Windows technically
// supports it as well, but not on Windows XP.
class PlatformUtilPosixTest : public PlatformUtilTest {
 public:
  void SetUpOnMainThread() override {
    PlatformUtilTest::SetUpOnMainThread();

    symlink_to_file_ = directory_.GetPath().AppendASCII("l_file.txt");
    ASSERT_TRUE(base::CreateSymbolicLink(existing_file_, symlink_to_file_));
    symlink_to_folder_ = directory_.GetPath().AppendASCII("l_folder");
    ASSERT_TRUE(base::CreateSymbolicLink(existing_folder_, symlink_to_folder_));
    symlink_to_nowhere_ = directory_.GetPath().AppendASCII("l_nowhere");
    ASSERT_TRUE(base::CreateSymbolicLink(nowhere_, symlink_to_nowhere_));
  }

 protected:
  base::FilePath symlink_to_file_;
  base::FilePath symlink_to_folder_;
  base::FilePath symlink_to_nowhere_;
};
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_POSIX)
// On all other Posix platforms, the symbolic link tests should work as
// expected.

IN_PROC_BROWSER_TEST_F(PlatformUtilPosixTest, OpenFileWithPosixSymlinks) {
  EXPECT_EQ(OPEN_SUCCEEDED, CallOpenItem(symlink_to_file_, OPEN_FILE));
  EXPECT_EQ(OPEN_FAILED_INVALID_TYPE,
            CallOpenItem(symlink_to_folder_, OPEN_FILE));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND,
            CallOpenItem(symlink_to_nowhere_, OPEN_FILE));
}

IN_PROC_BROWSER_TEST_F(PlatformUtilPosixTest, OpenFolderWithPosixSymlinks) {
  EXPECT_EQ(OPEN_SUCCEEDED, CallOpenItem(symlink_to_folder_, OPEN_FOLDER));
  EXPECT_EQ(OPEN_FAILED_INVALID_TYPE,
            CallOpenItem(symlink_to_file_, OPEN_FOLDER));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND,
            CallOpenItem(symlink_to_nowhere_, OPEN_FOLDER));
}
#endif  // BUILDFLAG(IS_POSIX)

}  // namespace platform_util
