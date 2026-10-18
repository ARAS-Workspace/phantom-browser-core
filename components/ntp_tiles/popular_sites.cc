// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/popular_sites.h"

namespace ntp_tiles {

PopularSites::Site::Site(const std::u16string& title,
                         const GURL& url,
                         const GURL& favicon_url,
                         const GURL& large_icon_url,
                         TileTitleSource title_source)
    : title(title),
      url(url),
      favicon_url(favicon_url),
      large_icon_url(large_icon_url),
      title_source(title_source),
      baked_in(false),
      default_icon_resource(-1) {}

PopularSites::Site::Site(const Site& other) = default;

PopularSites::Site::~Site() = default;

}  // namespace ntp_tiles
