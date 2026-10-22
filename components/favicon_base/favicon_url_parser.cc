// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_url_parser.h"

#include <string_view>

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/favicon_base/favicon_types.h"
#include "net/base/url_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/favicon_size.h"

namespace chrome {

namespace {

// Same as base::StringToInt() but guarantees that the output number is positive
// (greater than zero), returns false in all other cases.
[[nodiscard]] bool StringToPositiveInt(std::string_view input, int& output) {
  int result;
  if (!base::StringToInt(input, &result))
    return false;
  if (result <= 0)
    return false;
  output = result;
  return result;
}

// Parse with FaviconUrlFormat::kFavicon2 format.
bool ParseFaviconPathWithFavicon2Format(std::string_view path,
                                        chrome::ParsedFaviconPath* parsed) {
  if (path.empty())
    return false;

  GURL query_url = GURL("chrome://favicon2/").Resolve(path);

  *parsed = chrome::ParsedFaviconPath();

  for (net::QueryIterator it(query_url); !it.IsAtEnd(); it.Advance()) {
    const std::string_view key = it.GetKey();
    // Note: each of these keys can be used in chrome://favicon2 path. See file
    // "favicon_url_parser.h" for a description of what each one does.
    if (key == "allowGoogleServerFallback") {
      const std::string& val = it.GetUnescapedValue();
      if (!(val == "0" || val == "1"))
        return false;
      parsed->allow_favicon_server_fallback = val == "1";
    } else if (key == "showFallbackMonogram") {
      parsed->show_fallback_monogram = true;
    } else if (key == "forceLightMode") {
      parsed->force_light_mode = true;
    } else if (key == "iconUrl") {
      parsed->icon_url = it.GetUnescapedValue();
    } else if (key == "pageUrl") {
      parsed->page_url = it.GetUnescapedValue();
    } else if (key == "scaleFactor" &&
               !webui::ParseScaleFactor(it.GetUnescapedValue(),
                                        &parsed->device_scale_factor)) {
      return false;
    } else if (key == "size" && !StringToPositiveInt(it.GetUnescapedValue(),
                                                     parsed->size_in_dip)) {
      return false;
    } else if (key == "fallbackToHost") {
      const std::string& val = it.GetUnescapedValue();
      if (!(val == "0" || val == "1")) {
        return false;
      }
      parsed->fallback_to_host = val == "1";
    } else if (key == "forceEmptyDefaultFavicon") {
      const std::string& val = it.GetUnescapedValue();
      if (!(val == "0" || val == "1")) {
        return false;
      }
      parsed->force_empty_default_favicon = val == "1";
    }
  }

  if (parsed->page_url.empty() && parsed->icon_url.empty())
    return false;

  if (parsed->allow_favicon_server_fallback && parsed->page_url.empty()) {
    // Since the server is queried by page url, we'll fail if no non-empty page
    // url is provided and the fallback parameter is still set to true.
    NOTIMPLEMENTED();
    return false;
  }

  return true;
}

}  // namespace

ParsedFaviconPath::ParsedFaviconPath() = default;

ParsedFaviconPath::ParsedFaviconPath(const ParsedFaviconPath&) = default;

ParsedFaviconPath& ParsedFaviconPath::operator=(const ParsedFaviconPath&) =
    default;

bool ParseFaviconPath(const std::string& path,
                      FaviconUrlFormat format,
                      ParsedFaviconPath* parsed) {
  switch (format) {
    case FaviconUrlFormat::kFavicon2:
      return ParseFaviconPathWithFavicon2Format(path, parsed);
  }
  NOTREACHED();
}

}  // namespace chrome
