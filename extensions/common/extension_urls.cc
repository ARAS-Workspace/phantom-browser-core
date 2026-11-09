// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_urls.h"

#include <string_view>

#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extensions_client.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

bool IsSourceFromAnExtension(const std::u16string& source) {
  return GURL(source).SchemeIs(kExtensionScheme) ||
         base::StartsWith(source, u"extensions::",
                          base::CompareCase::SENSITIVE);
}

}  // namespace extensions

namespace extension_urls {

const char kChromeWebstoreBaseURL[] = "https://chrome.google.com/webstore";
const char kNewChromeWebstoreBaseURL[] = "https://chromewebstore.google.com/";
const char kChromeWebstoreUpdateURL[] =
    "https://clients2.google.com/service/update2/crx";
const char kChromeWebstoreApiURL[] = "https://chromewebstore.googleapis.com/";

GURL GetWebstoreLaunchURL() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetWebstoreBaseURL();
  return GURL(kChromeWebstoreBaseURL);
}

GURL GetNewWebstoreLaunchURL() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetNewWebstoreBaseURL();
  return GURL(kNewChromeWebstoreBaseURL);
}

std::string GetWebstoreItemDetailURLPrefix() {
  return GetNewWebstoreLaunchURL().spec() + "detail/";
}

GURL GetDefaultWebstoreUpdateUrl() {
  return GURL(kChromeWebstoreUpdateURL);
}

GURL GetWebstoreUpdateUrl() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetWebstoreUpdateURL();
  return GetDefaultWebstoreUpdateUrl();
}

GURL GetWebstoreReportAbuseUrl(const extensions::ExtensionId& extension_id,
                               const std::string& referrer_id) {
  return GURL(base::StringPrintf("%sdetail/%s/report?utm_source=%s",
                                 GetNewWebstoreLaunchURL().spec().c_str(),
                                 extension_id.c_str(), referrer_id.c_str()));
}

bool IsWebstoreDomain(const GURL& url) {
  return url.DomainIs(GetWebstoreLaunchURL().host()) ||
         url.DomainIs(GetNewWebstoreLaunchURL().host());
}

bool IsWebstoreOrigin(const url::Origin& origin) {
  return origin.IsSameOriginWith(GetWebstoreLaunchURL()) ||
         origin.IsSameOriginWith(GetNewWebstoreLaunchURL());
}

bool IsWebstoreUpdateUrl(const GURL& update_url) {
  GURL store_url = GetWebstoreUpdateUrl();
  return (update_url.host() == store_url.host() &&
          update_url.path() == store_url.path());
}

bool IsWebstoreApiUrl(const GURL& url) {
  url::Origin origin = url::Origin::Create(url);
  return origin.IsSameOriginWith(GURL(kChromeWebstoreApiURL));
}

bool IsBlocklistUpdateUrl(const GURL& url) {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->IsBlocklistUpdateURL(url);
  return false;
}

bool IsSafeBrowsingUrl(const GURL& url) {
  url::Origin origin = url::Origin::Create(url);
  std::string_view path = url.path();
  return origin.DomainIs("sb-ssl.google.com") ||
         origin.DomainIs("safebrowsing.googleapis.com") ||
         (origin.DomainIs("safebrowsing.google.com") &&
          base::StartsWith(path, "/safebrowsing",
                           base::CompareCase::SENSITIVE));
}

}  // namespace extension_urls
