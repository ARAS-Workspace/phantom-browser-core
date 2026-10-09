// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/url_identity.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

using Type = UrlIdentity::Type;
using DefaultFormatOptions = UrlIdentity::DefaultFormatOptions;
using FormatOptions = UrlIdentity::FormatOptions;
using TypeSet = UrlIdentity::TypeSet;
using testing::Eq;
using testing::Field;

namespace {
struct TestCase {
  GURL url;
  TypeSet allowed_types;
  FormatOptions options;
  UrlIdentity expected_result;
};

constexpr std::string_view kTestExtensionId =
    "0264075e-fd33-4a20-8484-b834afb0333d";
}  // namespace

class UrlIdentityTest : public testing::Test {
 protected:
  void SetUp() override {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    InstallExtension();
#endif
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  void InstallExtension() {
    std::string extension_id(kTestExtensionId);
    auto extension = extensions::ExtensionBuilder("Test Extension 1")
                         .SetID(extension_id)
                         .Build();
    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(profile());
    extension_registry->AddEnabled(extension);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

  Profile* profile() { return &testing_profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
#endif
};

TEST_F(UrlIdentityTest, AllowlistedTypesAreAllowed) {
  std::string extension_id(kTestExtensionId);
  std::vector<TestCase> test_cases = {
      {GURL("http://example.com"),
       {Type::kDefault},
       {},
       {
           .type = Type::kDefault,
           .name = u"http://example.com",
       }},
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      {extensions::Extension::GetBaseURLFromExtensionId(extension_id),
       {Type::kChromeExtension},
       {},
       {
           .type = Type::kChromeExtension,
           .name = u"Test Extension 1",
       }},
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      {GURL("file:///tmp/index.html"),
       {Type::kFile},
       {},
       {
           .type = Type::kFile,
           .name = u"file:///tmp/index.html",
       }},
  };

  for (const auto& test_case : test_cases) {
    UrlIdentity result = UrlIdentity::CreateFromUrl(
        profile(), test_case.url, test_case.allowed_types, test_case.options);
    EXPECT_EQ(result.name, test_case.expected_result.name);
    EXPECT_EQ(result.type, test_case.expected_result.type);
  }
}

TEST_F(UrlIdentityTest, DefaultFormatOptionsTest) {
  std::vector<TestCase> test_cases = {
      {GURL("https://example.com"),
       {Type::kDefault},
       {},
       {
           .type = Type::kDefault,
           .name = u"https://example.com",
       }},
      {GURL("https://example.com/123"),
       {Type::kDefault},
       {},
       {
           .type = Type::kDefault,
           .name = u"https://example.com",
       }},
      {GURL("https://example.com/123"),
       {Type::kDefault},
       {.default_options = {DefaultFormatOptions::kRawSpec}},
       {
           .type = Type::kDefault,
           .name = u"https://example.com/123",
       }},
      {GURL("https://example.com"),
       {Type::kDefault},
       {.default_options = {DefaultFormatOptions::kOmitCryptographicScheme}},
       {
           .type = Type::kDefault,
           .name = u"example.com",
       }},
      {GURL("https://abc.example.com"),
       {Type::kDefault},
       {.default_options = {DefaultFormatOptions::kHostname}},
       {
           .type = Type::kDefault,
           .name = u"abc.example.com",
       }},
      {GURL("http://user:pass@google.com/path"),
       {Type::kDefault},
       {.default_options =
            {DefaultFormatOptions::kOmitSchemePathAndTrivialSubdomains}},
       {
           .type = Type::kDefault,
           .name = u"google.com",
       }},
  };

  for (const auto& test_case : test_cases) {
    UrlIdentity result = UrlIdentity::CreateFromUrl(
        profile(), test_case.url, test_case.allowed_types, test_case.options);
    EXPECT_EQ(result.name, test_case.expected_result.name);
    EXPECT_EQ(result.type, test_case.expected_result.type);
  }
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
TEST_F(UrlIdentityTest, ChromeExtensionsOptionsTest) {
  std::string extension_id(kTestExtensionId);
  std::vector<TestCase> test_cases = {
      {extensions::Extension::GetBaseURLFromExtensionId(extension_id),
       {Type::kChromeExtension},
       {},
       {
           .type = Type::kChromeExtension,
           .name = u"Test Extension 1",
       }}};

  for (const auto& test_case : test_cases) {
    UrlIdentity result = UrlIdentity::CreateFromUrl(
        profile(), test_case.url, test_case.allowed_types, test_case.options);
    EXPECT_EQ(result.name, test_case.expected_result.name);
    EXPECT_EQ(result.type, test_case.expected_result.type);
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

TEST_F(UrlIdentityTest, FileOptionsTest) {
  std::vector<TestCase> test_cases = {
      {GURL("file:///tmp/index.html"),
       {Type::kFile},
       {},
       {
           .type = Type::kFile,
           .name = u"file:///tmp/index.html",
       }},
  };

  for (const auto& test_case : test_cases) {
    UrlIdentity result = UrlIdentity::CreateFromUrl(
        profile(), test_case.url, test_case.allowed_types, test_case.options);
    EXPECT_EQ(result.name, test_case.expected_result.name);
    EXPECT_EQ(result.type, test_case.expected_result.type);
  }
}
