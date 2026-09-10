// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/unit_test_util.h"

#include <optional>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/test_scope.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/re2/src/re2/re2.h"

namespace updater::test {
namespace {

std::string ToString(const std::string& s) {
  return s;
}

std::string ToString(const std::wstring& s) {
  return base::WideToUTF8(s);
}

}  // namespace

TEST(UnitTestUtil, ToString) {
  EXPECT_EQ(ToString("test"), ToString(L"test"));
}

TEST(UnitTestUtil, Processes) {
  auto print_processes_tester =
      [](const base::FilePath::StringType& process_name) {
        const std::string print_processes = PrintProcesses(process_name);
        const std::string regex_string = absl::StrFormat(
            R"(Found processes:\n)"
            R"(={72}\n(%s, pid=\d*, creation time=.*\n){2}={72}\n$)",
            ToString(process_name).c_str());
        bool is_match =
            re2::RE2::FullMatch(print_processes, re2::RE2(regex_string));
        if (!is_match) {
          ADD_FAILURE() << "regex:'" << regex_string << "'" << std::endl
                        << print_processes;
        }
        return is_match;
      };
  // Test the state of the process for the unit test process itself.
  base::FilePath::StringType unit_test = [] {
    base::FilePath unit_test_executable;
    base::PathService::Get(base::FILE_EXE, &unit_test_executable);
    return unit_test_executable.BaseName().value();
  }();
  EXPECT_TRUE(IsProcessRunning(unit_test));
  EXPECT_FALSE(WaitForProcessesToExit(unit_test, base::Milliseconds(1)));
  EXPECT_TRUE(print_processes_tester(unit_test));
}

TEST(UnitTestUtil, GetTestName) {
  EXPECT_EQ(GetTestName(), "UnitTestUtil.GetTestName");
}

// Enable the test to print the effective values for the test timeouts when
// debugging timeout issues.
TEST(UnitTestUtil, DISABLED_PrintTestTimeouts) {
  VLOG(0) << "action-timeout:"
          << TestTimeouts::action_timeout().InMilliseconds()
          << ", action-max-timeout:"
          << TestTimeouts::action_max_timeout().InMilliseconds()
          << ", test-launcher-timeout:"
          << TestTimeouts::test_launcher_timeout().InMilliseconds();
}

TEST(UnitTestUtil, DeleteFileAndEmptyParentDirectories) {
  EXPECT_FALSE(DeleteFileAndEmptyParentDirectories(std::nullopt));

  const base::FilePath path_not_found(FILE_PATH_LITERAL("path-not-found"));
  EXPECT_TRUE(DeleteFileAndEmptyParentDirectories(path_not_found));

  // Create something in temp so that `DeleteFileAndEmptyParentDirectories()`
  // does not delete the temp directory of this process because it is empty.
  base::ScopedTempDir a_temp_dir;
  ASSERT_TRUE(a_temp_dir.CreateUniqueTempDir());

  base::FilePath temp_path;
  ASSERT_TRUE(GetTempDir(&temp_path));

  // Create and delete the following path "some_dir/dir_in_dir/file_in_dir".
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath dir_in_dir;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      temp_dir.GetPath(), FILE_PATH_LITERAL("UnitTestUtil"), &dir_in_dir));
  base::FilePath file_in_dir;
  EXPECT_TRUE(CreateTemporaryFileInDir(dir_in_dir, &file_in_dir));
  EXPECT_TRUE(DeleteFileAndEmptyParentDirectories(file_in_dir));
  EXPECT_FALSE(base::DirectoryExists(temp_dir.GetPath()));
  EXPECT_TRUE(base::DirectoryExists(temp_path));
}

TEST(UnitTestUtil, IsJSONSubset) {
  std::optional<base::Value> needle =
      base::JSONReader::Read(R"({
    "key_a": "val_a",
    "key_b": {
      "key_b1": 1,
      "key_b2": ["x", ["y"]]
    },
    "missing_key": null
  })",
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(needle);

  std::optional<base::Value> haystack_match =
      base::JSONReader::Read(R"({
    "another_key": 50,
    "key_a": "val_a",
    "key_b": {
      "key_b1": 1,
      "key_b2": [4, "x", 19, ["rutabaga", "y", "beef"]],
      "extra": "cabbage"
    }
  })",
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(haystack_match);
  EXPECT_TRUE(IsJSONSubset(*needle, *haystack_match));

  std::optional<base::Value> haystack_missing_b =
      base::JSONReader::Read(R"({
    "key_a": "val_a"
  })",
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(haystack_missing_b);
  EXPECT_FALSE(IsJSONSubset(*needle, *haystack_missing_b));

  std::optional<base::Value> haystack_not_missing =
      base::JSONReader::Read(R"({
    "key_a": "val_a",
    "key_b": {
      "key_b1": 1,
      "key_b2": ["x", ["y"]]
    },
    "missing_key": "oops"
  })",
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(haystack_not_missing);
  EXPECT_FALSE(IsJSONSubset(*needle, *haystack_not_missing));

  std::optional<base::Value> haystack_out_of_order =
      base::JSONReader::Read(R"({
    "key_a": "val_a",
    "key_b": {
      "key_b1": 1,
      "key_b2": [["y"], "x"]
    }
  })",
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(haystack_out_of_order);
  EXPECT_FALSE(IsJSONSubset(*needle, *haystack_out_of_order));
}

TEST(UnitTestUtil, IsJSONSubsetNumericConversions) {
  EXPECT_TRUE(IsJSONSubset(base::Value(1.0), base::Value(1)));
  EXPECT_TRUE(IsJSONSubset(base::Value(1), base::Value(1.0)));
}

TEST(UnitTestUtil, IsJSONSubsetStringComparators) {
  base::Value needle_str(base::DictValue().Set("key", "VALUE"));
  base::Value haystack_str(base::DictValue().Set("key", "value"));

  EXPECT_FALSE(IsJSONSubset(needle_str, haystack_str));

  auto case_insensitive_comparator = [](const std::string& a,
                                        const std::string& b) {
    return base::EqualsCaseInsensitiveASCII(a, b);
  };

  EXPECT_TRUE(
      IsJSONSubset(needle_str, haystack_str, case_insensitive_comparator));

  auto never_match_comparator = [](const std::string& a, const std::string& b) {
    return false;
  };
  EXPECT_FALSE(IsJSONSubset(needle_str, haystack_str, never_match_comparator));

  std::optional<base::Value> needle_list = base::JSONReader::Read(
      R"(["A", "B"])", base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(needle_list);
  std::optional<base::Value> haystack_list = base::JSONReader::Read(
      R"(["a", "b"])", base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(haystack_list);

  EXPECT_FALSE(IsJSONSubset(*needle_list, *haystack_list));
  EXPECT_TRUE(
      IsJSONSubset(*needle_list, *haystack_list, case_insensitive_comparator));
}

}  // namespace updater::test
