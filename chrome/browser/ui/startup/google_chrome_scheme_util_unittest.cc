// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/google_chrome_scheme_util.h"

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace startup {

namespace {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char* kScheme = "google-chrome";
#else
const char* kScheme = "chromium";
#endif
}  // namespace

TEST(GoogleChromeSchemeUtilTest, StripGoogleChromeScheme) {
  base::test::ScopedFeatureList feature_list{features::kGoogleChromeScheme};

  std::string scheme_prefix = std::string(kScheme) + "://";
  using StringType = std::string;

  // Match
  {
    StringType expected =
        "example.com";
    StringType arg_str = scheme_prefix + expected;
    base::FilePath::StringViewType arg = arg_str;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
  }

  // No match (http)
  {
    StringType arg_str =
        "http://example.com";
    base::FilePath::StringViewType arg = arg_str;
    EXPECT_FALSE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, arg_str);
  }

  // Case insensitive
  {
    // Construct mixed case scheme.
    std::string mixed_scheme_ascii = kScheme;
    mixed_scheme_ascii[0] = toupper(mixed_scheme_ascii[0]);
    StringType mixed_scheme_prefix = mixed_scheme_ascii + "://";
    StringType expected = "example.com";
    StringType arg_str = mixed_scheme_prefix + expected;
    base::FilePath::StringViewType arg = arg_str;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
  }

  // Empty payload
  {
    StringType arg_str = scheme_prefix;
    base::FilePath::StringViewType arg = arg_str;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, StringType());
  }

  // Opaque and malformed separators supported
  {
    // google-chrome:example.com (missing //) - Now supported as opaque scheme.
    StringType expected = "example.com";
    StringType malformed = std::string(kScheme) + ":" + expected;
    base::FilePath::StringViewType arg = malformed;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
  }
  {
    // google-chrome:/example.com (missing one /) - Now supported as opaque
    // scheme.
    StringType expected = "/example.com";
    StringType malformed = std::string(kScheme) + ":" + expected;
    base::FilePath::StringViewType arg = malformed;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
  }
  {
    // google-chrome:http://www.example.com (opaque http)
    StringType expected = "http://www.example.com";
    StringType opaque = std::string(kScheme) + ":" + expected;
    base::FilePath::StringViewType arg = opaque;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
  }
  {
    // google-chrome:file:///tmp/test (opaque file)
    StringType expected = "file:///tmp/test";
    StringType opaque = std::string(kScheme) + ":" + expected;
    base::FilePath::StringViewType arg = opaque;
    EXPECT_TRUE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, expected);
  }

  // Feature disabled
  {
    base::test::ScopedFeatureList disabled_feature;
    disabled_feature.InitAndDisableFeature(features::kGoogleChromeScheme);

    StringType expected =
        "example.com";
    StringType arg_str = scheme_prefix + expected;
    base::FilePath::StringViewType arg = arg_str;
    // Should NOT strip if feature disabled.
    EXPECT_FALSE(StripGoogleChromeScheme(arg));
    EXPECT_EQ(arg, arg_str);
  }
}

TEST(GoogleChromeSchemeUtilTest, ExtractGoogleChromeSchemeInnerUrl) {
  base::test::ScopedFeatureList feature_list{features::kGoogleChromeScheme};

  std::string scheme = kScheme;

  // Standard case
  {
    GURL url(scheme + "://example.com");
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), GURL("example.com"));
  }

  // Opaque case
  {
    GURL url(scheme + ":http://example.com");
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), GURL("http://example.com"));
  }

  // File case
  {
    GURL url(scheme + ":file:///tmp/test");
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), GURL("file:///tmp/test"));
  }

  // No match
  {
    GURL url("http://example.com");
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_FALSE(result.has_value());
  }

  // Feature disabled
  {
    base::test::ScopedFeatureList disabled_feature;
    disabled_feature.InitAndDisableFeature(features::kGoogleChromeScheme);
    GURL url(scheme + "://example.com");
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_FALSE(result.has_value());
  }

  // Cross-scheme support (Strict mode - should fail)
  {
    std::string other_scheme;
    if (std::string(kScheme) == "google-chrome") {
      other_scheme = "chromium";
    } else {
      other_scheme = "google-chrome";
    }
    GURL url(base::StrCat({other_scheme, "://example.com"}));
    std::optional<GURL> result = ExtractGoogleChromeSchemeInnerUrl(url);
    EXPECT_FALSE(result.has_value());
  }
}

}  // namespace startup
