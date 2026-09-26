// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/file_data_source.h"

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_view_util.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class FileDataSourceCppTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath path = temp_dir_.GetPath().AppendASCII("testfile");
    ASSERT_TRUE(base::WriteFile(path, "123456"));
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_EQ(file.GetLength(), 6);
    file_data_source_ = std::make_unique<FileDataSource>(std::move(file));
    EXPECT_EQ(file_data_source_->GetLength(), 6u);

    // Create FileDataSource for a file-backed, and in-memory content-URI.
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FileDataSource> file_data_source_;
};

TEST_F(FileDataSourceCppTest, ReadAll) {
  std::vector<char> buf(6, 'x');
  file_data_source_->SetRange(0, 6u);
  auto result = file_data_source_->Read(0, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 6u);
  EXPECT_EQ(base::as_string_view(buf), "123456");
}

TEST_F(FileDataSourceCppTest, ReadInChunks) {
  std::vector<char> buf(3, 'x');
  file_data_source_->SetRange(0, 6);
  auto result = file_data_source_->Read(0, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 3u);
  EXPECT_EQ(base::as_string_view(buf), "123");

  result = file_data_source_->Read(3, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 3u);
  EXPECT_EQ(base::as_string_view(buf), "456");
}

TEST_F(FileDataSourceCppTest, SetRangeInvalid) {
  std::vector<char> buf(6, 'x');
  file_data_source_->SetRange(2, 1);
  EXPECT_EQ(file_data_source_->GetLength(), 0u);
  auto result = file_data_source_->Read(0, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_INVALID_ARGUMENT);
  EXPECT_EQ(result.bytes_read, 0u);
  EXPECT_EQ(base::as_string_view(buf), "xxxxxx");
}

TEST_F(FileDataSourceCppTest, ReadRange) {
  std::vector<char> buf(6, 'x');
  file_data_source_->SetRange(2, 4);
  EXPECT_EQ(file_data_source_->GetLength(), 2u);
  auto result = file_data_source_->Read(0, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 2u);
  EXPECT_EQ(base::as_string_view(buf), "34xxxx");
}

TEST_F(FileDataSourceCppTest, ReadFromOffset) {
  std::vector<char> buf(6, 'x');
  auto result = file_data_source_->Read(2, buf);
  EXPECT_EQ(result.result, MOJO_RESULT_OK);
  EXPECT_EQ(result.bytes_read, 4u);
  EXPECT_EQ(base::as_string_view(buf), "3456xx");
}

}  // namespace
}  // namespace mojo
