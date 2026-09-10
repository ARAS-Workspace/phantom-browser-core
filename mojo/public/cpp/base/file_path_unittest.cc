// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/test/gtest_util.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/file_path.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL(literal) FILE_PATH_LITERAL(literal)

namespace mojo_base {
namespace {

// Helper to construct RelativeFilePath structs that do not actually conform to
// the expected preconditions.
mojom::RelativeFilePathPtr CreateArbitraryRelativeFilePath(
    const base::FilePath& file_path) {
  auto mojo_file_path = mojom::RelativeFilePath::New();
  mojo_file_path->path = file_path.value();
  return mojo_file_path;
}

TEST(FilePathTest, File) {
  base::FilePath dir(FILE_PATH_LITERAL("hello"));
  base::FilePath file = dir.Append(FILE_PATH_LITERAL("world"));
  base::FilePath file_out;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::FilePath>(file, file_out));
  ASSERT_EQ(file, file_out);
}

TEST(FilePathTest, RelativeFilePath) {
  base::FilePath dir(FILE_PATH_LITERAL("hello"));

  base::FilePath file = dir.Append(FILE_PATH_LITERAL("world"));
  base::FilePath file_out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::RelativeFilePath>(
      file, file_out));
  ASSERT_EQ(file, file_out);

  base::FilePath ignored_out;
  {
    const base::FilePath in_path(FPL("/vmlinuz"));
    ASSERT_TRUE(in_path.IsAbsolute());

    EXPECT_CHECK_DEATH(
        mojo::test::SerializeAndDeserialize<mojom::RelativeFilePath>(
            in_path, ignored_out));

    auto in_struct = CreateArbitraryRelativeFilePath(in_path);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::RelativeFilePath>(
        in_struct, ignored_out));
  }

  {
    const base::FilePath in_path(FPL("relative/path/../with/traversals"));
    ASSERT_TRUE(!in_path.IsAbsolute());
    ASSERT_TRUE(in_path.ReferencesParent());

    EXPECT_CHECK_DEATH(
        mojo::test::SerializeAndDeserialize<mojom::RelativeFilePath>(
            in_path, ignored_out));

    auto in_struct = CreateArbitraryRelativeFilePath(in_path);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::RelativeFilePath>(
        in_struct, ignored_out));
  }
}

}  // namespace
}  // namespace mojo_base
