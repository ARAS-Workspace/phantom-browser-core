// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/url_pattern_util.h"

#include <string_view>

#include "base/check.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"
#include "url/url_util.h"

namespace lookalikes {

namespace {

std::string Unescape(const std::string& url) {
  std::string unescaped_str(url);
  const int kMaxLoopIterations = 1024;
  size_t old_size = 0;
  int loop_var = 0;
  do {
    old_size = unescaped_str.size();
    unescaped_str = base::UnescapeBinaryURLComponent(unescaped_str);
  } while (old_size != unescaped_str.size() &&
           ++loop_var <= kMaxLoopIterations);

  return unescaped_str;
}

std::string Escape(const std::string& url) {
  std::string escaped_str;
  // The escaped string is larger so allocate double the length to reduce the
  // chance of the string being grown.
  escaped_str.reserve(url.length() * 2);
  for (size_t i = 0; i < url.length(); i++) {
    unsigned char c = static_cast<unsigned char>(url[i]);
    if (c <= ' ' || c > '~' || c == '#' || c == '%') {
      escaped_str += '%';
      base::AppendHexEncodedByte(c, escaped_str);
    } else {
      escaped_str += c;
    }
  }

  return escaped_str;
}

std::string RemoveConsecutiveChars(std::string_view str, const char c) {
  std::string output;
  // Output is at most the length of the original string.
  output.reserve(str.size());

  size_t i = 0;
  while (i < str.size()) {
    output.append(1, str[i++]);
    if (str[i - 1] == c) {
      while (i < str.size() && str[i] == c) {
        i++;
      }
    }
  }

  return output;
}

void GenerateHostVariantsToCheckV4(const std::string& host,
                                   std::vector<std::string>* hosts) {
  // Per the Safe Browsing Protocol v2 spec, we try the host, and also up to 4
  // hostnames formed by starting with the last 5 components and successively
  // removing the leading component.  The last component isn't examined alone,
  // since it's the TLD or a subcomponent thereof.
  //
  // Note that we don't need to be clever about stopping at the "real" eTLD --
  // the data on the server side has been filtered to ensure it will not
  // blocklist a whole TLD, and it's not significantly slower on our side to
  // just check too much.
  //
  // Also note that because we have a simple blocklist, not some sort of complex
  // allowlist-in-blocklist or vice versa, it doesn't matter what order we check
  // these in.
  const size_t kMaxHostsToCheck = 4;
  bool skipped_last_component = false;
  for (std::string::const_reverse_iterator i(host.rbegin());
       i != host.rend() && hosts->size() < kMaxHostsToCheck; ++i) {
    if (*i == '.') {
      if (skipped_last_component) {
        hosts->push_back(std::string(i.base(), host.end()));
      } else {
        skipped_last_component = true;
      }
    }
  }
  hosts->push_back(host);
}

}  // namespace

void CanonicalizeUrl(const GURL& url,
                     std::string* canonicalized_hostname,
                     std::string* canonicalized_path,
                     std::string* canonicalized_query) {
  DCHECK(url.is_valid());

  // We only canonicalize "normal" URLs.
  if (!url.IsStandard()) {
    return;
  }

  // Following canonicalization steps are excluded since url parsing takes care
  // of those :-
  // 1. Remove any tab (0x09), CR (0x0d), and LF (0x0a) chars from url.
  //    (Exclude escaped version of these chars).
  // 2. Normalize hostname to 4 dot-seperated decimal values.
  // 3. Lowercase hostname.
  // 4. Resolve path sequences "/../" and "/./".

  // That leaves us with the following :-
  // 1. Remove fragment in URL.
  GURL url_without_fragment;
  GURL::Replacements f_replacements;
  f_replacements.ClearRef();
  f_replacements.ClearUsername();
  f_replacements.ClearPassword();
  url_without_fragment = url.ReplaceComponents(f_replacements);

  // 2. Do URL unescaping until no more hex encoded characters exist.
  std::string url_unescaped_str(Unescape(url_without_fragment.spec()));
  std::string_view url_unescaped_str_view(url_unescaped_str);
  url::Parsed parsed = url::ParseStandardUrl(url_unescaped_str);

  // 3. In hostname, remove all leading and trailing dots.
  std::string_view host;
  if (parsed.host.is_nonempty()) {
    host = url_unescaped_str_view.substr(parsed.host.begin, parsed.host.len);
  }

  std::string_view host_without_end_dots =
      base::TrimString(host, ".", base::TrimPositions::TRIM_ALL);

  // 4. In hostname, replace consecutive dots with a single dot.
  std::string host_without_consecutive_dots(
      RemoveConsecutiveChars(host_without_end_dots, '.'));

  // 5. In path, replace runs of consecutive slashes with a single slash.
  std::string_view path;
  if (parsed.path.is_nonempty()) {
    path = url_unescaped_str_view.substr(parsed.path.begin, parsed.path.len);
  }
  std::string path_without_consecutive_slash(RemoveConsecutiveChars(path, '/'));

  url::Replacements<char> hp_replacements;
  hp_replacements.SetHostStr(host_without_consecutive_dots);
  hp_replacements.SetPathStr(path_without_consecutive_slash);

  std::string url_unescaped_with_can_hostpath;
  url::StdStringCanonOutput output(&url_unescaped_with_can_hostpath);
  url::Parsed temp_parsed;
  url::ReplaceComponents(url_unescaped_str_view, parsed, hp_replacements,
                         nullptr, &output, &temp_parsed);
  output.Complete();

  // 6. Step needed to revert escaping done in url::ReplaceComponents.
  url_unescaped_with_can_hostpath = Unescape(url_unescaped_with_can_hostpath);

  // 7. After performing all above steps, percent-escape all chars in url which
  // are <= ASCII 32, >= 127, #, %. Escapes must be uppercase hex characters.
  std::string escaped_canon_url_str(Escape(url_unescaped_with_can_hostpath));
  url::Parsed final_parsed = url::ParseStandardUrl(escaped_canon_url_str);

  if (canonicalized_hostname && final_parsed.host.is_nonempty()) {
    *canonicalized_hostname = escaped_canon_url_str.substr(
        final_parsed.host.begin, final_parsed.host.len);
  }
  if (canonicalized_path && final_parsed.path.is_nonempty()) {
    *canonicalized_path = escaped_canon_url_str.substr(final_parsed.path.begin,
                                                       final_parsed.path.len);
  }
  if (canonicalized_query && final_parsed.query.is_nonempty()) {
    *canonicalized_query = escaped_canon_url_str.substr(
        final_parsed.query.begin, final_parsed.query.len);
  }
}

void GenerateHostVariantsToCheck(const std::string& host,
                                 std::vector<std::string>* hosts) {
  hosts->clear();

  if (host.empty()) {
    return;
  }

  GenerateHostVariantsToCheckV4(host, hosts);
}

void GeneratePathVariantsToCheck(const std::string& path,
                                 const std::string& query,
                                 std::vector<std::string>* paths) {
  paths->clear();

  if (path.empty()) {
    return;
  }

  // Per the Safe Browsing Protocol v2 spec, we try the exact path with/without
  // the query parameters, and also up to 4 paths formed by starting at the root
  // and adding more path components.
  //
  // As with the hosts above, it doesn't matter what order we check these in.
  const size_t kMaxPathsToCheck = 4;
  for (std::string::const_iterator i(path.begin());
       i != path.end() && paths->size() < kMaxPathsToCheck; ++i) {
    if (*i == '/') {
      paths->push_back(std::string(path.begin(), i + 1));
    }
  }

  if (!paths->empty() && paths->back() != path) {
    paths->push_back(path);
  }

  if (!query.empty()) {
    paths->push_back(path + "?" + query);
  }
}

}  // namespace lookalikes
