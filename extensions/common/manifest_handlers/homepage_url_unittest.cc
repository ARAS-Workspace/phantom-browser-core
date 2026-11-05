// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/manifest_url_handlers.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

using HomepageURLManifestTest = ManifestTest;

TEST_F(HomepageURLManifestTest, ParseHomepageURLs) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("homepage_url_valid.json"));

  const Testcase testcases[] = {
      Testcase("homepage_url_empty.json", errors::kInvalidHomepageURL),
      Testcase("homepage_url_invalid.json", errors::kInvalidHomepageURL),
      Testcase("homepage_url_bad_schema.json", errors::kInvalidHomepageURL)};
  RunTestcases(testcases, ExpectType::kError);
}

TEST_F(HomepageURLManifestTest, GetHomepageURL) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("homepage_url_valid.json"));
  EXPECT_EQ(GURL("http://foo.com#bar"),
            ManifestURL::GetHomepageURL(extension.get()));

  // An extension updating from the web store has no web store homepage.
  extension = LoadAndExpectSuccess("homepage_url_google_hosted.json");
  EXPECT_EQ(GURL(), ManifestURL::GetHomepageURL(extension.get()));

  extension = LoadAndExpectSuccess("homepage_url_externally_hosted.json");
  EXPECT_EQ(GURL(), ManifestURL::GetHomepageURL(extension.get()));
}

}  // namespace extensions
