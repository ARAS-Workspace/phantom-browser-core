// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_url_parser.h"

#include <memory>

#include "components/favicon_base/favicon_types.h"
#include "testing/gtest/include/gtest/gtest.h"

class FaviconUrlParserTest : public testing::Test {
 public:
  FaviconUrlParserTest() = default;

  FaviconUrlParserTest(const FaviconUrlParserTest&) = delete;
  FaviconUrlParserTest& operator=(const FaviconUrlParserTest&) = delete;

  ~FaviconUrlParserTest() override = default;
};

TEST_F(FaviconUrlParserTest, Favicon2ParsingSizeParam) {
  chrome::ParsedFaviconPath parsed;

  EXPECT_TRUE(chrome::ParseFaviconPath("?size=32&pageUrl=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_EQ(32, parsed.size_in_dip);

  EXPECT_FALSE(chrome::ParseFaviconPath("?size=abc&pageUrl=https%3A%2F%2Fg.com",
                                        chrome::FaviconUrlFormat::kFavicon2,
                                        &parsed));

  EXPECT_FALSE(chrome::ParseFaviconPath("?size=-32&pageUrl=https%3A%2F%2Fg.com",
                                        chrome::FaviconUrlFormat::kFavicon2,
                                        &parsed));
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingScaleFactorParam) {
  chrome::ParsedFaviconPath parsed;

  EXPECT_TRUE(
      chrome::ParseFaviconPath("?scaleFactor=2.1x&pageUrl=https%3A%2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_EQ(2.1f, parsed.device_scale_factor);

  // Missing 'x' in scale factor (parsing error).
  EXPECT_FALSE(
      chrome::ParseFaviconPath("?scaleFactor=2.1&pageUrl=https%3A%2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));

  // Negative scale factor (parsing error).
  EXPECT_FALSE(
      chrome::ParseFaviconPath("?scaleFactor=-2.1x&pageUrl=https%3A%2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));

  // Scale factor zero (parsing error).
  EXPECT_FALSE(
      chrome::ParseFaviconPath("?scaleFactor=0x&pageUrl=https%3A%2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingUrlParams) {
  chrome::ParsedFaviconPath parsed;

  EXPECT_TRUE(
      chrome::ParseFaviconPath("?iconUrl=https%3A%2F%2Fg.com%2Ffavicon.ico",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_EQ(parsed.icon_url, "https://g.com/favicon.ico");
  EXPECT_EQ(parsed.page_url, "");

  EXPECT_TRUE(chrome::ParseFaviconPath("?pageUrl=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_EQ(parsed.icon_url, "");
  EXPECT_EQ(parsed.page_url, "https://g.com");
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingAllowFallbackParam) {
  chrome::ParsedFaviconPath parsed;

  EXPECT_FALSE(chrome::ParseFaviconPath(
      "?allowGoogleServerFallback=invalid&pageUrl=https%"
      "3A%2F%2Fg.com",
      chrome::FaviconUrlFormat::kFavicon2, &parsed));

  EXPECT_TRUE(
      chrome::ParseFaviconPath("?allowGoogleServerFallback=0&pageUrl=https%3A%"
                               "2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_FALSE(parsed.allow_favicon_server_fallback);

  EXPECT_TRUE(
      chrome::ParseFaviconPath("?allowGoogleServerFallback=1&pageUrl=https%3A%"
                               "2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_TRUE(parsed.allow_favicon_server_fallback);
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingShowFallbackMonogram) {
  chrome::ParsedFaviconPath parsed;

  parsed.show_fallback_monogram = true;
  EXPECT_TRUE(chrome::ParseFaviconPath("?pageUrl=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_FALSE(parsed.show_fallback_monogram);

  parsed.show_fallback_monogram = false;
  EXPECT_TRUE(
      chrome::ParseFaviconPath("?showFallbackMonogram&pageUrl=https%3A%"
                               "2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_TRUE(parsed.show_fallback_monogram);
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingForceLightMode) {
  chrome::ParsedFaviconPath parsed;

  parsed.force_light_mode = true;
  EXPECT_TRUE(chrome::ParseFaviconPath("?pageUrl=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_FALSE(parsed.force_light_mode);

  parsed.force_light_mode = false;
  EXPECT_TRUE(
      chrome::ParseFaviconPath("?forceLightMode&pageUrl=https%3A%"
                               "2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_TRUE(parsed.force_light_mode);
}

TEST_F(FaviconUrlParserTest, Favicon2ParsingForceEmptyDefaultFavicon) {
  chrome::ParsedFaviconPath parsed;

  // `forceEmptyDefaultFavicon` omitted.
  parsed.force_empty_default_favicon = true;
  EXPECT_TRUE(chrome::ParseFaviconPath("?pageUrl=https%3A%2F%2Fg.com",
                                       chrome::FaviconUrlFormat::kFavicon2,
                                       &parsed));
  EXPECT_FALSE(parsed.force_empty_default_favicon);

  // `forceEmptyDefaultFavicon` set to "foo" (invalid value).
  parsed.force_empty_default_favicon = false;
  EXPECT_FALSE(chrome::ParseFaviconPath(
      "?forceEmptyDefaultFavicon=foo&pageUrl=https%3A%2F%2Fg.com",
      chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_FALSE(parsed.force_empty_default_favicon);

  // `forceEmptyDefaultFavicon` set to "0".
  parsed.force_empty_default_favicon = true;
  EXPECT_TRUE(
      chrome::ParseFaviconPath("?forceEmptyDefaultFavicon=0&pageUrl=https%3A%"
                               "2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_FALSE(parsed.force_empty_default_favicon);

  // `forceEmptyDefaultFavicon` set to "1".
  parsed.force_empty_default_favicon = false;
  EXPECT_TRUE(
      chrome::ParseFaviconPath("?forceEmptyDefaultFavicon=1&pageUrl=https%3A%"
                               "2F%2Fg.com",
                               chrome::FaviconUrlFormat::kFavicon2, &parsed));
  EXPECT_TRUE(parsed.force_empty_default_favicon);
}
