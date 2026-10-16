// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/url_data_source.h"

class Profile;

// Serves chrome-untrusted://new-tab-page/* sources which can return content
// from outside the chromium codebase. The chrome-untrusted://new-tab-page/*
// sources can only be embedded in the chrome://new-tab-page by using an
// <iframe>.
//
// Offers the following helpers to embed content into chrome://new-tab-page in a
// generalized way:
//   * chrome-untrusted://new-tab-page/image?<url>: Behaves like an img element
//       with src set to <url>.
//   * chrome-untrusted://new-tab-page/background_image?<url>: Behaves like an
//       element that has <url> set as the background image, such that the image
//       will cover the entire element.
//   * chrome-untrusted://new-tab-page/custom_background_image?<params>: Similar
//       to background_image but allows for custom styling. <params> are of the
//       form <key>=<value>. The following keys are supported:
//         * url:       background image URL.
//         * url2x:     (optional) URL to a higher res background image.
//         * size:      (optional) CSS background-size property.
//         * repeatX:   (optional) CSS background-repeat-x property.
//         * repeatY:   (optional) CSS background-repeat-y property.
//         * positionX: (optional) CSS background-position-x property.
//         * positionY: (optional) CSS background-position-y property.
//   Each of those helpers only accept URLs with HTTPS or chrome-untrusted:.
class UntrustedSource : public content::URLDataSource {
 public:
  explicit UntrustedSource(Profile* profile);
  ~UntrustedSource() override;
  UntrustedSource(const UntrustedSource&) = delete;
  UntrustedSource& operator=(const UntrustedSource&) = delete;

  // content::URLDataSource:
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  bool AllowCaching() override;
  bool ShouldReplaceExistingSource() override;
  bool ShouldServeMimeTypeAsContentTypeHeader() override;
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;

 private:
  void ServeBackgroundImage(const GURL& url,
                            const GURL& url_2x,
                            std::string_view size,
                            std::string_view repeat_x,
                            std::string_view repeat_y,
                            std::string_view position_x,
                            std::string_view position_y,
                            std::string_view scrim_display,
                            content::URLDataSource::GotDataCallback callback);
  bool IsURLAllowed(const GURL& url);
  bool IsURLBlockedByPolicy(const GURL& url);

  raw_ptr<Profile, FlakyDanglingUntriaged> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_
