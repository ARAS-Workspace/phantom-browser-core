// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

void WriteTestNativeHostManifest(const base::FilePath& target_dir,
                                 const std::string& host_name,
                                 const base::FilePath& host_path,
                                 bool user_level,
                                 bool supports_native_initiated_connections) {
  auto manifest = base::DictValue()
                      .Set("name", host_name)
                      .Set("description", "Native Messaging Echo Test")
                      .Set("type", "stdio")
                      .Set("path", host_path.AsUTF8Unsafe())
                      .Set("supports_native_initiated_connections",
                           supports_native_initiated_connections);

  manifest.Set("allowed_origins",
               base::ListValue().Append(base::StringPrintf(
                   "chrome-extension://%s/",
                   ScopedTestNativeMessagingHost::kExtensionId)));

  base::FilePath manifest_path = target_dir.AppendASCII(host_name + ".json");
  JSONFileValueSerializer serializer(manifest_path);
  ASSERT_TRUE(serializer.Serialize(manifest));

}

}  // namespace

const char ScopedTestNativeMessagingHost::kHostName[] =
    "com.google.chrome.test.echo";
const char ScopedTestNativeMessagingHost::kBinaryMissingHostName[] =
    "com.google.chrome.test.host_binary_missing";
const char ScopedTestNativeMessagingHost::
    kSupportsNativeInitiatedConnectionsHostName[] =
        "com.google.chrome.test.inbound_native_echo";
const char ScopedTestNativeMessagingHost::kExtensionId[] =
    "knldjmfmopnpolahpmmgbagdohdnhkik";

ScopedTestNativeMessagingHost::ScopedTestNativeMessagingHost() = default;

void ScopedTestNativeMessagingHost::RegisterTestHost(bool user_level) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  base::FilePath test_user_data_dir;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_user_data_dir));
  test_user_data_dir = test_user_data_dir.AppendASCII("native_messaging")
                           .AppendASCII("native_hosts");

  path_override_ = std::make_unique<base::ScopedPathOverride>(
      user_level ? chrome::DIR_USER_NATIVE_MESSAGING
                 : chrome::DIR_NATIVE_MESSAGING,
      temp_dir_.GetPath());

  base::CopyFile(test_user_data_dir.AppendASCII("echo.py"),
                 temp_dir_.GetPath().AppendASCII("echo.py"));

#if BUILDFLAG(IS_POSIX)
  base::FilePath host_path = temp_dir_.GetPath().AppendASCII("echo.py");
  ASSERT_TRUE(base::SetPosixFilePermissions(
      host_path, base::FILE_PERMISSION_READ_BY_USER |
                     base::FILE_PERMISSION_WRITE_BY_USER |
                     base::FILE_PERMISSION_EXECUTE_BY_USER));
#endif
  ASSERT_NO_FATAL_FAILURE(WriteTestNativeHostManifest(
      temp_dir_.GetPath(), kHostName, host_path, user_level, false));

  ASSERT_NO_FATAL_FAILURE(WriteTestNativeHostManifest(
      temp_dir_.GetPath(), kBinaryMissingHostName,
      test_user_data_dir.AppendASCII("missing_nm_binary.exe"), user_level,
      false));

  ASSERT_NO_FATAL_FAILURE(WriteTestNativeHostManifest(
      temp_dir_.GetPath(), kSupportsNativeInitiatedConnectionsHostName,
      host_path, user_level, true));
}

ScopedTestNativeMessagingHost::~ScopedTestNativeMessagingHost() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::ignore = temp_dir_.Delete();
}

}  // namespace extensions
