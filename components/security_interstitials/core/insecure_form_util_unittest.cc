// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/insecure_form_util.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using security_interstitials::IsInsecureFormActionOnSecureSource;

class InsecureFormUtilTest : public ::testing::Test {
 public:
};

TEST_F(InsecureFormUtilTest, IsInsecureFormActionOnSecureSource) {
  EXPECT_TRUE(IsInsecureFormActionOnSecureSource(
      url::Origin::Create(GURL("https://example.com")),
      GURL("http://example.com")));

  EXPECT_FALSE(IsInsecureFormActionOnSecureSource(
      url::Origin::Create(GURL("http://example.com")),
      GURL("http://example.com")));
  EXPECT_FALSE(IsInsecureFormActionOnSecureSource(
      url::Origin::Create(GURL("http://example.com")),
      GURL("https://example.com")));
  EXPECT_FALSE(IsInsecureFormActionOnSecureSource(
      url::Origin::Create(GURL("https://example.com")),
      GURL("https://example.com")));

  // Opaque https source with insecure action still counts.
  EXPECT_TRUE(IsInsecureFormActionOnSecureSource(
      url::Origin::Create(GURL("https://example.com")).DeriveNewOpaqueOrigin(),
      GURL("http://example.com")));

  // Other combinations do not.
  EXPECT_FALSE(IsInsecureFormActionOnSecureSource(
      url::Origin::Create(GURL("https://example.com")).DeriveNewOpaqueOrigin(),
      GURL("https://example.com")));
  EXPECT_FALSE(IsInsecureFormActionOnSecureSource(
      url::Origin::Create(GURL("http://example.com")).DeriveNewOpaqueOrigin(),
      GURL("http://example.com")));
  EXPECT_FALSE(IsInsecureFormActionOnSecureSource(url::Origin(),
                                                  GURL("http://example.com")));
}
