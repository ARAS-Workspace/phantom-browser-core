// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/security_settings_provider.h"

#include "base/feature_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/web_ui_data_source.h"

namespace settings {

void AddSecurityData(content::WebUIDataSource* html_source) {
  html_source->AddBoolean(
      "enableBundledSecuritySettingsSecureDnsV2",
      base::FeatureList::IsEnabled(
          safe_browsing::kBundledSecuritySettingsSecureDnsV2));
}

}  // namespace settings
