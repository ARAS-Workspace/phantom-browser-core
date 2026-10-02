// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_page_user_data.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/data_protection/utils.h"
#include "content/public/browser/page.h"

#if BUILDFLAG(ENTERPRISE_WATERMARK)
#include "chrome/browser/enterprise/watermark/settings.h"
#endif

namespace enterprise_data_protection {

// static
void DataProtectionPageUserData::UpdateDataControlsScreenshotState(
    content::Page& page,
    const std::string& identifier,
    bool allow) {
  auto* ud = GetForPage(page);
  if (ud) {
    ud->data_controls_settings_.allow_screenshots = allow;
    return;
  }

  UrlSettings data_controls_settings;
  data_controls_settings.allow_screenshots = allow;
  CreateForPage(page, identifier, data_controls_settings);
}

DataProtectionPageUserData::DataProtectionPageUserData(
    content::Page& page,
    const std::string& identifier,
    UrlSettings data_controls_settings)
    : PageUserData(page),
      identifier_(identifier),
      data_controls_settings_(std::move(data_controls_settings)) {}

DataProtectionPageUserData::~DataProtectionPageUserData() = default;

UrlSettings DataProtectionPageUserData::settings() const {
  return data_controls_settings_;
}

PAGE_USER_DATA_KEY_IMPL(DataProtectionPageUserData);

}  // namespace enterprise_data_protection
