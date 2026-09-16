// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_select_helper.h"

#include <stddef.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/test/test_dialog_model_host.h"
#include "ui/shell_dialogs/selected_file_info.h"

using blink::mojom::FileChooserParams;

class FileSelectHelperTest : public testing::Test {
 public:
  FileSelectHelperTest() = default;

  FileSelectHelperTest(const FileSelectHelperTest&) = delete;
  FileSelectHelperTest& operator=(const FileSelectHelperTest&) = delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("file_select_helper");
    ASSERT_TRUE(base::PathExists(data_dir_));
  }

  std::unique_ptr<ui::TestDialogModelHost> CreateDialogHost(
      scoped_refptr<FileSelectHelper> file_select_helper) {
    base::FilePath dir(FILE_PATH_LITERAL("dir"));
    base::FilePath file1(FILE_PATH_LITERAL("file1"));
    base::FilePath file2(FILE_PATH_LITERAL("file2"));
    std::vector<blink::mojom::FileChooserFileInfoPtr> selected_files;
    std::vector<std::u16string> base_subdirs;
    selected_files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(file1, u"file1", base_subdirs)));
    selected_files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(file2, u"file2", base_subdirs)));

    auto model = file_select_helper->CreateConfirmationDialog(
        u"dir", std::move(selected_files),
        base::BindLambdaForTesting(
            [&](std::vector<blink::mojom::FileChooserFileInfoPtr>
                    selected_files) {
              ++callback_count_;
              selected_files_ = std::move(selected_files);
            }));
    return std::make_unique<ui::TestDialogModelHost>(std::move(model));
  }

  // The path to input data used in tests.
  base::FilePath data_dir_;

  std::vector<blink::mojom::FileChooserFileInfoPtr> selected_files_;
  int callback_count_ = 0;
};

TEST_F(FileSelectHelperTest, IsAcceptTypeValid) {
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid("a/b"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid("abc/def"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid("abc/*"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid(".a"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid(".abc"));

  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("."));
  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("/"));
  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("ABC/*"));
  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("abc/def "));
}

#if BUILDFLAG(IS_MAC)
TEST_F(FileSelectHelperTest, ZipPackage) {
  // Zip the package.
  const char app_name[] = "CalculatorFake.app";
  base::FilePath src = data_dir_.Append(app_name);
  base::FilePath dest = FileSelectHelper::ZipPackage(src);
  ASSERT_FALSE(dest.empty());
  ASSERT_TRUE(base::PathExists(dest));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Unzip the package into a temporary directory.
  base::CommandLine cl(base::FilePath("/usr/bin/unzip"));
  cl.AppendArg(dest.value().c_str());
  cl.AppendArg("-d");
  cl.AppendArg(temp_dir.GetPath().value().c_str());
  std::string output;
  EXPECT_TRUE(base::GetAppOutput(cl, &output));

  // Verify that several key files haven't changed.
  const auto files_to_verify = std::to_array<std::string_view>({
      "Contents/Info.plist",
      "Contents/MacOS/Calculator",
      "Contents/_CodeSignature/CodeResources",
  });
  for (std::string_view relative_path : files_to_verify) {
    base::FilePath orig_file = src.Append(relative_path);
    base::FilePath final_file =
        temp_dir.GetPath().Append(app_name).Append(relative_path);
    EXPECT_TRUE(base::ContentsEqual(orig_file, final_file));
  }
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(FileSelectHelperTest, GetSanitizedFileName) {
  // The empty path should be preserved.
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("")),
            FileSelectHelper::GetSanitizedFileName(base::FilePath()));

  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("ascii.txt")),
            FileSelectHelper::GetSanitizedFileName(
                base::FilePath(FILE_PATH_LITERAL("ascii.txt"))));
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("trailing-spaces_")),
            FileSelectHelper::GetSanitizedFileName(
                base::FilePath(FILE_PATH_LITERAL("trailing-spaces "))));
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("path_components_in_name")),
            FileSelectHelper::GetSanitizedFileName(
                base::FilePath(FILE_PATH_LITERAL("path/components/in/name"))));

  // Invalid UTF-8
  base::FilePath::CharType kBadName[] = {'\xe3', '\x81', '\x81',
                                         '\x81', '\x82', '\0'};
  base::FilePath bad_filename(kBadName);
  ASSERT_FALSE(bad_filename.empty());
  // The only thing we are testing is that if the source filename was non-empty,
  // the resulting filename is also not empty. Invalid encoded filenames can
  // cause conversions to fail. Such failures shouldn't cause the resulting
  // filename to disappear.
  EXPECT_FALSE(FileSelectHelper::GetSanitizedFileName(bad_filename).empty());
}

TEST_F(FileSelectHelperTest, LastSelectedDirectory) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  const int index = 0;

  const base::FilePath dir_path_1 = data_dir_.AppendASCII("dir1");
  const base::FilePath dir_path_2 = data_dir_.AppendASCII("dir2");
  const base::FilePath file_path_1 = dir_path_1.AppendASCII("file1.txt");
  const base::FilePath file_path_2 = dir_path_1.AppendASCII("file2.txt");
  const base::FilePath file_path_3 = dir_path_2.AppendASCII("file3.txt");
  std::vector<base::FilePath> files;  // Both in dir1.
  files.push_back(file_path_1);
  files.push_back(file_path_2);
  std::vector<base::FilePath> dirs;
  dirs.push_back(dir_path_1);
  dirs.push_back(dir_path_2);

  // Modes where the parent of the selection is remembered.
  const std::vector<FileChooserParams::Mode> modes = {
      FileChooserParams::Mode::kOpen, FileChooserParams::Mode::kOpenMultiple,
      FileChooserParams::Mode::kSave,
  };

  for (const auto& mode : modes) {
    file_select_helper->dialog_mode_ = mode;

    file_select_helper->FileSelected(ui::SelectedFileInfo(file_path_1), index);
    EXPECT_EQ(dir_path_1, profile.last_selected_directory());

    file_select_helper->FileSelected(ui::SelectedFileInfo(file_path_2), index);
    EXPECT_EQ(dir_path_1, profile.last_selected_directory());

    file_select_helper->FileSelected(ui::SelectedFileInfo(file_path_3), index);
    EXPECT_EQ(dir_path_2, profile.last_selected_directory());

    file_select_helper->MultiFilesSelected(
        ui::FilePathListToSelectedFileInfoList(files));
    EXPECT_EQ(dir_path_1, profile.last_selected_directory());
  }

  // Type where the selected folder itself is remembered.
  file_select_helper->dialog_mode_ = FileChooserParams::Mode::kUploadFolder;

  file_select_helper->FileSelected(ui::SelectedFileInfo(dir_path_1), index);
  EXPECT_EQ(dir_path_1, profile.last_selected_directory());

  file_select_helper->FileSelected(ui::SelectedFileInfo(dir_path_2), index);
  EXPECT_EQ(dir_path_2, profile.last_selected_directory());

  file_select_helper->MultiFilesSelected(
      ui::FilePathListToSelectedFileInfoList(dirs));
  EXPECT_EQ(dir_path_1, profile.last_selected_directory());
}

// The following tests depend on the enterprise cloud content analysis feature
// set.

TEST_F(FileSelectHelperTest, GetFileTypesFromAcceptType) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<std::u16string> accept_types{
      // normal file extension
      u".mp4",
      // file extension with some chinese
      u".斤拷锟",
      // file extension with fire emoji
      u".🔥",
      // mime type
      u"image/png",
      // non-ascii mime type which should be ignored
      u"text/斤拷锟"};

  std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> file_type_info =
      file_select_helper->GetFileTypesFromAcceptType(accept_types);

  std::vector<std::vector<base::FilePath::StringType>> expected_extensions{
      std::vector<base::FilePath::StringType>{
          "mp4", "斤拷锟", "🔥", "png"}};
  ASSERT_EQ(expected_extensions, file_type_info->extensions);
}

// This test depends on platform-specific mappings from mime types to file
// extensions in PlatformMimeUtil. It would seem that Linux does not offer a way
// to get extensions, and our Windows implementation still needs to be updated.
#if BUILDFLAG(IS_MAC)
TEST_F(FileSelectHelperTest, MultipleFileExtensionsForMime) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<std::u16string> accept_types{u"application/vnd.ms-powerpoint"};
  std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> file_type_info =
      file_select_helper->GetFileTypesFromAcceptType(accept_types);

  std::vector<base::FilePath::StringType> expected_extensions {
    "ppt", "pot", "pps"
  };
  std::sort(expected_extensions.begin(), expected_extensions.end());

  ASSERT_EQ(file_type_info->extensions.size(), 1u);
  std::vector<base::FilePath::StringType> actual_extensions =
      file_type_info->extensions[0];
  std::sort(actual_extensions.begin(), actual_extensions.end());

  EXPECT_EQ(expected_extensions, actual_extensions);
}
#endif

TEST_F(FileSelectHelperTest, ConfirmationDialog) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  // Cancel should be initially focused.
  auto host = CreateDialogHost(file_select_helper);
  EXPECT_EQ(host->GetInitiallyFocusedField(),
            host->GetId(ui::TestDialogModelHost::ButtonId::kCancel));

  // Accept should run callback with all files.
  ui::TestDialogModelHost::Accept(std::move(host));
  EXPECT_EQ(callback_count_, 1);
  EXPECT_EQ(selected_files_.size(), 2u);

  // Cancel should run callback with no files.
  host = CreateDialogHost(file_select_helper);
  ui::TestDialogModelHost::Cancel(std::move(host));
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(selected_files_.size(), 0u);

  // Closing should invokes cancel.
  host = CreateDialogHost(file_select_helper);
  ui::TestDialogModelHost::Close(std::move(host));
  EXPECT_EQ(callback_count_, 3);
  EXPECT_EQ(selected_files_.size(), 0u);
}
